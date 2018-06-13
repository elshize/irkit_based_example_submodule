#include <chrono>
#include <iostream>

#include <irkit/coding/varbyte.hpp>
#include <irkit/index.hpp>
#include <irkit/index/posting_list.hpp>

int main(int argc, char** argv)
{
    assert(argc == 3);
    irk::fs::path index_dir(argv[1]);
    irk::v2::inverted_index_mapped_data_source data(index_dir);
    irk::v2::inverted_index_view index_view(&data,
        irk::coding::varbyte_codec<long>{},
        irk::coding::varbyte_codec<long>{});

    std::string term(argv[2]);

    auto postings = index_view.postings(term);
    std::vector<std::pair<long, long>> vec_tmp(postings.begin(), postings.end());
    auto start_interval = std::chrono::steady_clock::now();
    std::vector<std::pair<long, long>> vec(postings.begin(), postings.end());
    auto end_interval = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
        end_interval - start_interval);
    for (auto posting : vec) {
        std::cout << posting.first << "\t"
                  << posting.second << std::endl;
    }
    long time =
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    std::cout << "Read " << postings.size() << " postings"
              << " for term: " << term << " (" << time << "ms; "
              << ((double)time / postings.size()) << "ms per posting)"
              << std::endl;
    return 0;
}
