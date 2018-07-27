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

bool order(const Posting<long, long>& lhs, 
            const Posting<long, long>& rhs)
{
    return lhs.score > rhs.score;
}

auto taat(int k, const std::vector<post_list_t>& query,
            long collection_size){
    std::vector<long> acc(collection_size, 0);
    std::vector<Posting<long, long> > topk;
    long threshold = 0;
    for(size_t i = 0; i < query.size(); ++i){
        for(const auto& posting : query[i]){
            acc[posting.document()] += posting.payload();
        }
    }
    for(size_t i = 0; i < acc.size(); ++i){
        if(acc[i] > threshold){
            Posting<long, long> p {long(i), acc[i]};
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

void initialize(std::vector<std::vector<long> >& maxTable,
                std::vector<std::vector<std::bitset<8> > >& subTable,
                const std::vector<post_list_t>& query, 
                long collection_size, int block_size){
    int subsize = block_size / 8;
    for(const auto& pList : query){
        std::vector<long> bmax(collection_size / block_size + 1, 0);
        std::vector<std::bitset<8> > subblocks;
        auto p = pList.begin();
        for(size_t i = 0; i < bmax.size(); ++i){
            std::bitset<8> pb;
            while(p != pList.end() && p.document() < (i + 1) * block_size){
                if(p.payload() > bmax[i]) {bmax[i] = p.payload();}
                pb.set((p.document() - i * block_size) / subsize);
                p.moveto(p.document() + 1);
            }
            subblocks.push_back(pb);
        }
        maxTable.push_back(bmax);
        subTable.push_back(subblocks);
    }
}

auto live_block_count(const std::vector<std::vector<long> >& maxTable,
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
            std::bitset<8> pb(0xff);
            for(const auto& sub : subTable){
                pb &= sub[i];
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
    std::map<int, std::vector<double> > ql;
    std::cout << "Query\t"
              << "Rough Rate\t" 
              << "Advanced Rate\t"
              << "Sub Blocks Rate\t"
              << "Original Rate\n";
    while(getline(query_in, line)){
        std::stringstream aQuery(line);
        std::string term;
        std::vector<post_list_t> query_postings;
        irk::porter2_stemmer stemmer;
        std::vector<std::vector<long> > maxTable;
        std::vector<std::vector<std::bitset<8> > > subTable;
        long og = 0;
        while(aQuery >> term){
            term = stemmer.stem(term);
            auto id = index_view.term_id(term);
            if (id.has_value()) {
                query_postings.push_back(
                index_view.scored_postings(id.value()));
            }
        }
        if(query_postings.size() == 0){
            std::cout << num++ << "\tCan't find any result for query '"
                      << line << "'\n";
            continue;
        }
        // std::chrono::nanoseconds elapsed(0);
        // auto start_interval = std::chrono::steady_clock::now();
        auto result = taat(k, query_postings, index_view.collection_size());
        // auto end_interval = std::chrono::steady_clock::now();
        // elapsed += std::chrono::duration_cast<std::chrono::nanoseconds>
        //             (end_interval - start_interval);
        // long time = 
        //     std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        // std::cout << "Top " << k << " Results in " 
        //             << time << " ms\n"
        //             << "Document\tScore\n";
        // print_posting(result);
        long threshold = (result.end() - 1)->score;
        initialize(maxTable, subTable, query_postings, index_view.collection_size(), 64);
        std::vector<double> live_rate = 
            live_block_count(maxTable, subTable, query_postings, threshold, og, 64);
        double og_rate = (double)og / maxTable[0].size() * 100;
        std::cout << num++ << "\t"
                << live_rate[0] << "%\t"
                << live_rate[1] << "%\t"
                << live_rate[2] << "%\t"
                << og_rate << "%" << std::endl;
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
        // std::cout << "Query " << num++ << ": " << line 
        //           << "\nDID\t" << "Score\n"; 
        // for(auto posting : result){
        //     std::cout << posting.document << "\t"
        //               << posting.score << std::endl;
        // }
        // std::cout << std::endl;

        sumRough += live_rate[0];
        sumAdvanced += live_rate[1];
        sumSub += live_rate[2];
        sumOg += og_rate;
    }
    
    std::cout << "Average for " << --num << " Queries:\n"
              << "Average Rough Live Rate: " << sumRough / num << "%"
              << "\nAverage Advanced Live Rate: " << sumAdvanced / num << "%"
              << "\nAverage Sub Blocks Live Rate: " << sumSub / num << "%" 
              << "\nAverage Original Blocks Rate: " << sumOg / num << "%" << std::endl;

    std::cout << "Average for Different Querie Length:\n"
              << "Length\t" << "Rough Rate\t" << "Advanced Rate\t" << "Subs Rate\t" << "Original Rate"<< std::endl;
    for(const auto& len : ql){
        std::cout << len.first << "\t";
        for(size_t i = 1; i < len.second.size(); ++i){ std::cout << len.second[i] / len.second[0] << "%\t"; }
        std::cout << std::endl;
    }

}