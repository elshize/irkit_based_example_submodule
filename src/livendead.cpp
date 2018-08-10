#include <iostream>
#include <chrono>
#include <bitset>
#include <string>
#include <sstream>
#include <map>

#include <irkit/coding/stream_vbyte.hpp>
#include <irkit/index.hpp>
#include <irkit/io.hpp>
#include <irkit/memoryview.hpp>
#include <irkit/index/source.hpp>
#include <irkit/index/types.hpp>
#include <irkit/parsing/stemmer.hpp>
//#define private public
#include <irkit/index/posting_list.hpp>

using std::uint32_t;
using irk::index::document_t;
using doc_list_t = irk::index::block_document_list_view<
    irk::stream_vbyte_codec<document_t>>;
using payload_list_t = irk::index::block_payload_list_view<uint32_t,
    irk::stream_vbyte_codec<uint32_t>>;
using post_list_t = irk::posting_list_view<doc_list_t, payload_list_t>;

template<class Document, class Score>
struct Posting {
    Document document;
    Score score;
};

void print_posting(const std::vector<Posting<long, long> >& postings){
    for(auto p : postings){
        std::cout << p.document << "\t\t"
             << p.score << std::endl;
    }
}

bool order(const Posting<document_t, std::uint32_t>& lhs, 
            const Posting<document_t, std::uint32_t>& rhs)
{
    return lhs.score > rhs.score;
}

auto taat(int k, const std::vector<post_list_t>& query,
            long collection_size){
    std::vector<std::uint32_t> acc(collection_size, 0);
    std::vector<Posting<document_t, std::uint32_t> > topk;
    std::uint32_t threshold = 0;
    for(const auto& term : query){
        for(const auto& posting : term){
            acc[posting.document()] += posting.payload();
        }
    }
    for(document_t i = 0; i < acc.size(); ++i){
        if(acc[i] > threshold){
            Posting<document_t, std::uint32_t> p {i, acc[i]};
            topk.push_back(p);
            if(topk.size() <= k) std::push_heap(topk.begin(), topk.end(), order);
            else{
                std::pop_heap(topk.begin(), topk.end(), order);
                topk.pop_back();
            }
            threshold = topk.size() == k ? topk[0].score : threshold;
        }
    }
    std::sort(topk.begin(), topk.end(), order);
    return topk;
}

void initialize(std::vector<std::vector<uint32_t> >& maxTable,
                std::vector<std::vector<std::bitset<8> > >& subTable,
                const std::vector<post_list_t>& query, 
                long collection_size, int block_size){
    int subsize = block_size / 8;
    maxTable.resize(query.size());
    subTable.resize(query.size());
    for(uint32_t term = 0; term < query.size(); term++) {
        const auto& pList = query[term];
        auto& bmax = maxTable[term];
        bmax.resize(collection_size / block_size + 1, 0);
        auto& subblocks = subTable[term];
        subblocks.resize(collection_size / block_size + 1);
        auto p = pList.begin();
        const auto& end = pList.end();
        for(size_t i = 0; i < bmax.size(); ++i){
            while(p != end && p.document() < (i + 1) * block_size){
                bmax[i] = std::max(p.payload(), bmax[i]);
                subblocks[i].set((p.document() - i * block_size) / subsize);
                p.moveto(p.document() + 1);
            }
        }
    }
}

auto live_block_count(const std::vector<std::vector<uint32_t> >& maxTable,
                      const std::vector<std::vector<std::bitset<8> > >& subTable,
                      const std::vector<post_list_t>& query, 
                      long threshold, long& og, int block_size){
    long liveBlock = 0;
    long liveBlockSub = 0;
    long liveSub = 0;
    std::vector<double> result;
    std::bitset<8> empty;
    for(size_t i = 0; i < maxTable[0].size(); ++i){
        long temp_score = 0;
        for(const auto& bmax : maxTable){
            temp_score += bmax[i];
        }
        if(temp_score > 0){ ++og; }
        if(temp_score >= threshold){
            ++liveBlock;
            std::bitset<8> pb;
            for(size_t j = 0; j < 8; ++j){
                long sub_temp = 0;
                for(size_t k = 0; k < subTable.size(); ++k){
                    sub_temp += subTable[k][i][j] * maxTable[k][i];
                }
                if(sub_temp >= threshold){ pb.set(j); }
            }  
            if(pb != empty){
                ++liveBlockSub;
                liveSub += pb.count();
            }
        } 
    }
    double rateBlock = (double)liveBlock / maxTable[0].size() * 100;
    double rateBlockSub = (double)liveBlockSub / maxTable[0].size() * 100;
    double rateSub = (double)liveSub / (maxTable[0].size() * 8) * 100;
    result.push_back(rateBlock);
    result.push_back(rateBlockSub);
    result.push_back(rateSub);
    return result;
}

int main(int argc, char** argv){
    assert(argc == 4);
    irk::fs::path index_dir(argv[1]);
    std::ifstream term_in(irk::index::term_map_path(index_dir).c_str());
    std::ifstream title_in(irk::index::title_map_path(index_dir).c_str());
    irk::inverted_index_mapped_data_source data(index_dir, "bm25");
    irk::inverted_index_view index_view(&data);

    std::ifstream query_in(argv[2]);
    if (!query_in) {
		std::cerr << "Failed to open query.txt\n";
		exit(1);
	}

    std::string line;
    int k = std::stoi(argv[3]);
    int num = 1;
    double sumRough = 0;
    double sumAdvanced = 0;
    double sumSub = 0;
    double sumOg = 0;
    double sumTotal = 0;
    double sumTaat = 0;
    double sumInit = 0;
    double sumCount = 0;
    std::map<int, std::vector<double> > ql;
    std::cout << "Query\t"
              << "Rough Rate\t" 
              << "Advanced Rate\t"
              << "Sub Blocks Rate\t"
              << "Original Rate\t"
              << "Total Time\t"
              << "Taat Time\t"
              << "Init Time\t"
              << "Count Time\n";
    while(getline(query_in, line)){
        std::stringstream aQuery(line);
        std::string term;
        std::vector<post_list_t> query_postings;
        irk::porter2_stemmer stemmer;
        std::vector<std::vector<uint32_t> > maxTable;
        std::vector<std::vector<std::bitset<8> > > subTable;
        long og = 0;
        std::vector<std::string> query;

        while(aQuery >> term){
            term = stemmer.stem(term);
            query.push_back(term);
        }
        if(query.size() == 0){
            std::cout << num++ << "\tCan't find any result for query '"
                      << line << "'\n";
            continue;
        }

        auto start_time = std::chrono::steady_clock::now();
        for(const auto& term : query){
            auto id = index_view.term_id(term);
            if (id.has_value()) {
                query_postings.push_back(
                index_view.scored_postings(id.value()));
            }
        }
        auto result = taat(k, query_postings, index_view.collection_size());
        auto after_taat = std::chrono::steady_clock::now();

        long threshold = (result.end() - 1)->score;
        initialize(maxTable, subTable, query_postings, index_view.collection_size(), 64);
        auto after_init = std::chrono::steady_clock::now();

        std::vector<double> live_rate = 
            live_block_count(maxTable, subTable, query_postings, threshold, og, 64);
        auto end_time = std::chrono::steady_clock::now();

        auto total = std::chrono::duration_cast<std::chrono::milliseconds>(
                    end_time - start_time);
        auto taat = std::chrono::duration_cast<std::chrono::milliseconds>(
                    after_taat - start_time);
        auto init = std::chrono::duration_cast<std::chrono::milliseconds>(
                    after_init - after_taat);
        auto count = std::chrono::duration_cast<std::chrono::milliseconds>(
                    end_time - after_init);
        double og_rate = (double)og / maxTable[0].size() * 100;
        std::cout << num++ << "\t"
                << live_rate[0] << "%\t"
                << live_rate[1] << "%\t"
                << live_rate[2] << "%\t"
                << og_rate << "%\t" 
                << total.count() << "ms\t"
                << taat.count() << "ms\t"
                << init.count() << "ms\t"
                << count.count() << "ms\t" << std::endl;
        if(ql[query_postings.size()].size() == 0){
            ql[query_postings.size()].push_back(1);
            for(const auto& rate : live_rate){
                ql[query_postings.size()].push_back(rate);
            } 
            ql[query_postings.size()].push_back(og_rate);
        }
        else{
            ++ql[query_postings.size()][0];
            for(size_t i = 0; i < live_rate.size(); ++i){
                ql[query_postings.size()][i + 1] += live_rate[i];
            } 
            ql[query_postings.size()][4] += og_rate;
        }

        sumRough += live_rate[0];
        sumAdvanced += live_rate[1];
        sumSub += live_rate[2];
        sumOg += og_rate;
        sumTotal += total.count();
        sumTaat += taat.count();
        sumInit += init.count();
        sumCount += count.count();
    }
    
    std::cout << "Average for " << --num << " Queries:\n"
              << "Average Rough Live Rate: " << sumRough / num << "%"
              << "\nAverage Advanced Live Rate: " << sumAdvanced / num << "%"
              << "\nAverage Sub Blocks Live Rate: " << sumSub / num << "%" 
              << "\nAverage Original Blocks Rate: " << sumOg / num << "%" 
              << "\nAverage Total Time: " << sumTotal / num << "ms" 
              << "\nAverage Taat Time: " << sumTaat / num << "ms"
              << "\nAverage Init Time: " << sumInit / num << "ms"
              << "\nAverage Count Time: " << sumCount / num << "ms" << std::endl;

    std::cout << "Average for Different Querie Length:\n"
              << "Length\t" << "Rough Rate\t" << "Advanced Rate\t" << "Subs Rate\t" << "Original Rate"<< std::endl;
    for(const auto& len : ql){
        std::cout << len.first << "\t";
        for(size_t i = 1; i < len.second.size(); ++i){ std::cout << len.second[i] / len.second[0] << "%\t"; }
        std::cout << std::endl;
    }

}