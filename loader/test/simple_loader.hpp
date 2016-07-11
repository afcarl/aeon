#ifndef SIMPLE_LOADER_HPP
#define SIMPLE_LOADER_HPP

#include <string>

#include "buffer_in.hpp"
#include "batch_iterator.hpp"

class simple_loader : public BatchIterator
{
public:
    simple_loader(const std::string& repoDir);

    int start() { return 0; }
    void stop() {}

    void read(buffer_in_array& dest) override;
    void reset() override;

private:
    std::string     _repo;

};

#endif // SIMPLE_LOADER_HPP
