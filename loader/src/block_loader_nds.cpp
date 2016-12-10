/*
 Copyright 2016 Nervana Systems Inc.
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/

// inspired by https://techoverflow.net/blog/2013/03/15/c-simple-http-download-using-libcurl-easy-api/

// apt-get install libcurl4-openssl-dev
#include <curl/curl.h>
#include <curl/easy.h>
#include <curl/curlbuild.h>

#include "json.hpp"
#include "block_loader_nds.hpp"
#include "interface.hpp"

using namespace std;
using namespace nervana;

size_t write_data(void* ptr, size_t size, size_t nmemb, void* stream)
{
    stringstream& ss = *(stringstream*)stream;
    // callback used by curl.  writes data from ptr into the
    // stringstream passed in to `stream`.

    ss.write((const char*)ptr, size * nmemb);
    return size * nmemb;
}

block_loader_nds::block_loader_nds(const std::string& baseurl, const std::string& token, int collection_id, uint32_t block_size,
                                   int shard_count, int shard_index)
    : block_loader(block_size)
    , m_baseurl(baseurl)
    , m_token(token)
    , m_collection_id(collection_id)
    , m_shard_count(shard_count)
    , m_shard_index(shard_index)
{
    affirm(shard_index < shard_count, "shard index must be less then shard count");

    curl_global_init(CURL_GLOBAL_ALL);

    load_metadata();
}

block_loader_nds::~block_loader_nds()
{
    if (m_future.valid())
    {
        m_future.wait();
    }
    curl_global_cleanup();
}

void block_loader_nds::load_block(nervana::buffer_in_array& dest, uint32_t block_num)
{
    buffer_t buffer;
    if (m_future.valid())
    {
        buffer = m_future.get();
    }
    else
    {
        buffer = fetch_block(block_num);
    }
    auto it = buffer.begin();
    for (int i = 0; i < block_size() && it != buffer.end(); i++)
    {
        for (int j = 0; j < dest.size(); j++)
        {
            dest[j]->add_item(move(*it++));
        }
    }
}

block_loader_nds::buffer_t block_loader_nds::fetch_block(uint32_t block_num)
{
    // not much use in mutlithreading here since in most cases, our next step is
    // to shuffle the entire BufferPair, which requires the entire buffer loaded.
    buffer_t rc;

    // get data from url and write it into cpio_stream
    stringstream stream;
    get(load_block_url(block_num), stream);

    // parse cpio_stream into dest one record (consisting of multiple elements) at a time
    nervana::cpio::reader reader(stream);
    while (stream)
    {
        vector<char> buffer;
        string filename = reader.read(buffer);
        if (filename == cpio::CPIO_TRAILER || filename == cpio::AEON_TRAILER)
        {
            break;
        }
        rc.push_back(move(buffer));
    }
    return rc;
}

void block_loader_nds::get(const string& url, stringstream& stream)
{
    // reuse curl connection across requests
    void* m_curl = curl_easy_init();

    // given a url, make an HTTP GET request and fill stream with
    // the body of the response

    curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &stream);
    // curl_easy_setopt(m_curl, CURLOPT_VERBOSE, 1L);

    // Perform the request, res will get the return code
    CURLcode res = curl_easy_perform(m_curl);

    // Check for errors
    long http_code = 0;
    curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code != 200 || res != CURLE_OK)
    {
        stringstream ss;
        ss << "HTTP GET on \n'" << url << "' failed. ";
        ss << "status code: " << http_code;
        if (res != CURLE_OK)
        {
            ss << " curl return: " << curl_easy_strerror(res);
        }

        curl_easy_cleanup(m_curl);
        throw std::runtime_error(ss.str());
    }

    curl_easy_cleanup(m_curl);
}

const string block_loader_nds::load_block_url(uint32_t block_num)
{
    stringstream ss;
    ss << m_baseurl << "/macrobatch/?";
    ss << "macro_batch_index=" << block_num;
    ss << "&macro_batch_max_size=" << m_block_size;
    ss << "&collection_id=" << m_collection_id;
    ss << "&shard_count=" << m_shard_count;
    ss << "&shard_index=" << m_shard_index;
    ss << "&token=" << m_token;
    return ss.str();
}

const string block_loader_nds::metadata_url()
{
    stringstream ss;
    ss << m_baseurl << "/object_count/?";
    ss << "macro_batch_max_size=" << m_block_size;
    ss << "&collection_id=" << m_collection_id;
    ss << "&shard_count=" << m_shard_count;
    ss << "&shard_index=" << m_shard_index;
    ss << "&token=" << m_token;
    return ss.str();
}

void block_loader_nds::load_metadata()
{
    // fetch metadata and store in local attributes

    stringstream json_stream;
    get(metadata_url(), json_stream);
    string         json_str = json_stream.str();
    nlohmann::json metadata;
    try
    {
        metadata = nlohmann::json::parse(json_str);
    }
    catch (std::exception& ex)
    {
        stringstream ss;
        ss << "exception parsing metadata from nds ";
        ss << metadata_url() << " " << ex.what() << " ";
        ss << json_str;
        throw std::runtime_error(ss.str());
    }

    nervana::interface::config::parse_value(m_object_count, "record_count", metadata, nervana::interface::config::mode::REQUIRED);
    nervana::interface::config::parse_value(m_block_count, "macro_batch_per_shard", metadata,
                                            nervana::interface::config::mode::REQUIRED);
}

uint32_t block_loader_nds::object_count()
{
    return m_object_count;
}

uint32_t block_loader_nds::block_count()
{
    return m_block_count;
}

void block_loader_nds::prefetch_block(uint32_t block_num)
{
    m_future = async(&block_loader_nds::fetch_block, this, block_num);
}
