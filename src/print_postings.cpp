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
    for (auto posting : postings) {
        std::cout << posting.document() << "\t"
                  << posting.payload() << std::endl;
    }
    std::cout << "Found " << postings.size() << " postings"
        << " for term: " << term << std::endl;
    return 0;
}
