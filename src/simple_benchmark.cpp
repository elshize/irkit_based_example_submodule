#include <chrono>
#include <iostream>

#include <irkit/coding/varbyte.hpp>
#include <irkit/index.hpp>
#include <irkit/index/posting_list.hpp>
#include <irkit/io.hpp>
#include <irkit/memoryview.hpp>

class inverted_index_inmemory_data_source {

public:
  explicit inverted_index_inmemory_data_source(fs::path dir) {
    irk::io::enforce_exist(irk::index::doc_ids_path(dir));
    irk::io::enforce_exist(irk::index::doc_counts_path(dir));
    irk::io::enforce_exist(irk::index::doc_ids_off_path(dir));
    irk::io::enforce_exist(irk::index::doc_counts_off_path(dir));
    irk::io::enforce_exist(irk::index::term_doc_freq_path(dir));
    irk::io::enforce_exist(irk::index::term_map_path(dir));
    irk::io::enforce_exist(irk::index::title_map_path(dir));

    irk::io::load_data(irk::index::doc_ids_path(dir), documents_);
    irk::io::load_data(irk::index::doc_counts_path(dir), counts_);
    irk::io::load_data(irk::index::doc_ids_off_path(dir), document_offsets_);
    irk::io::load_data(irk::index::doc_counts_off_path(dir), count_offsets_);
    irk::io::load_data(irk::index::term_doc_freq_path(dir),
                       term_collection_frequencies_);
    irk::io::load_data(irk::index::term_map_path(dir), term_map_);
    irk::io::load_data(irk::index::title_map_path(dir), title_map_);
  }

  irk::memory_view documents_view() const {
    return irk::make_memory_view(documents_.data(), documents_.size());
  }

  irk::memory_view counts_view() const {
    return irk::make_memory_view(counts_.data(), counts_.size());
  }

  irk::memory_view document_offsets_view() const {
    return irk::make_memory_view(document_offsets_.data(),
                                 document_offsets_.size());
  }

  irk::memory_view count_offsets_view() const {
    return irk::make_memory_view(count_offsets_.data(), count_offsets_.size());
  }

  irk::memory_view term_collection_frequencies_view() const {
    return irk::make_memory_view(term_collection_frequencies_.data(),
                                 term_collection_frequencies_.size());
  }

  irk::memory_view term_map_source() const {
    return irk::make_memory_view(term_map_.data(), term_map_.size());
  }

  irk::memory_view title_map_source() const {
    return irk::make_memory_view(title_map_.data(), title_map_.size());
  }

private:
  std::vector<char> documents_;
  std::vector<char> counts_;
  std::vector<char> document_offsets_;
  std::vector<char> count_offsets_;
  std::vector<char> term_collection_frequencies_;
  std::vector<char> term_map_;
  std::vector<char> title_map_;
};

int main(int argc, char** argv)
{
    assert(argc == 2);
    irk::fs::path index_dir(argv[1]);
    std::ifstream term_in(irk::index::term_map_path(index_dir));
    std::ifstream title_in(irk::index::title_map_path(index_dir));
    inverted_index_inmemory_data_source data(index_dir);
    // Alternatively, you can use:
    // irk::v2::inverted_index_mapped_data_source data(index_dir);
    // Note that a constructor inverted_index_view(data) will work
    // in the future to make it simpler. (It actually works for
    // irk::v2::inverted_index_mapped_data_source)
    irk::v2::inverted_index_view index_view(
        data.documents_view(),
        data.counts_view(),
        data.document_offsets_view(),
        data.count_offsets_view(),
        data.term_collection_frequencies_view(),
        term_in,
        title_in,
        irk::coding::varbyte_codec<long>{},
        irk::coding::varbyte_codec<long>{});

    std::chrono::nanoseconds elapsed(0);
    long posting_count;

    for (long term_id = 0; term_id < index_view.terms().size(); term_id++) {
        auto postings = index_view.postings(term_id);
        std::vector<std::pair<long, long>> vec(postings.size());
        auto start_interval = std::chrono::steady_clock::now();
        std::copy(postings.begin(), postings.end(), vec.begin());
        auto end_interval = std::chrono::steady_clock::now();
        elapsed += std::chrono::duration_cast<std::chrono::nanoseconds>(
            end_interval - start_interval);
        posting_count += vec.size();
        assert(postings.size() == vec.size());
    }

    long time =
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    double avg_time = (double) time / index_view.terms().size();
    double time_per_posting = (double) time / posting_count;
    std::cout << "Avg traversal time: " << avg_time << "ms (" << time_per_posting
              << "ms/posting)" << std::endl;
    return 0;
}
