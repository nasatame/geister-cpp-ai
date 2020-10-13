#include "player.hpp"
#include "random.hpp"
#include "geister.hpp"
#include "simulator.hpp"
#include <vector>
#include <algorithm>
#include <iostream>

#ifndef PLAYOUT_COUNT
#define PLAYOUT_COUNT 300
#endif

#ifndef PLAYONE_COUNT
#define PLAYONE_COUNT 3
#endif


class MonteCarloMethodPlayer: public Player{
    uint32_t playoutCount = PLAYOUT_COUNT;
    cpprefjp::random_device rd;
    std::mt19937 mt; 
public:
    MonteCarloMethodPlayer(): mt(rd()){
    }

    virtual std::string decideRed(){
        std::uniform_int_distribution<int> serector(0, pattern.size() - 1);
        return pattern[serector(mt)];
    }

    virtual std::string decideHand(std::string res){
        game = Geister(res);
        // 合法手の列挙と着手の初期化
        std::vector<Hand> legalMoves = game.getLegalMove1st();
        Hand action = legalMoves[0];
        // 勝率記録用配列
        std::vector<double> rewards(legalMoves.size(), 0.0);
        int playout_all = 0;
        std::vector<int> playout(legalMoves.size(), 0.0);
        
        // 合法手の数だけ子局面を生成する
        std::vector<Simulator> children;
        for(int i = 0; i < legalMoves.size(); i++){
            Geister next(game);
            next.move(legalMoves[i]);
            Simulator sim(next);
            children.push_back(sim);
        }

        //UCB1 = μi + α√((log n) / ni)
        //一回のプレイアウトはPLAYONE_COUNT
        //αは0.1から1まで試す
        //μi:手iの報酬期待値 = rewards[i]
        //α:調整様パラメータ
        //n:全体のプレイアウト回数 = playout_all
        //ni:手iのプレイアウト回数 = playout[i]

        constexpr double alpha = 1;

        //とりあえず各自一回はプレイしてあげる。
        playout_all = legalMoves.size() * 3;
        for(int i = 0; i < legalMoves.size(); i++){
            rewards[i] = children[i].run(3);
            playout[i] = 3;
            rewards[i] /= playout[i];
        }

        const int playout_count = legalMoves.size() * 100;

        // 規定回数のプレイアウトを実行
        for(int i = legalMoves.size() * 3; i < playout_count; i+=PLAYONE_COUNT){
            
            int run_i = -1;
            double ucb1 = -10000000;

            for(int k = 0; k < legalMoves.size(); k++){
                if(ucb1 < rewards[k] + alpha * std::sqrt(std::log(playout_all) / playout[k])){
                    ucb1 = rewards[k] + alpha * std::sqrt(std::log(playout_all) / playout[k]);
                    run_i = k;
                }
            }
            
            if(run_i == -1){
                std::cerr << "errro -1" << std::endl;
                std::exit(-1);
            }

            playout_all += PLAYONE_COUNT;
            rewards[run_i] = rewards[run_i] * playout[run_i] + children[run_i].run(PLAYONE_COUNT);
            playout[run_i] += PLAYONE_COUNT;
            rewards[run_i] /= playout[run_i];
        }

        // 報酬が最大の手を探す
        std::vector<double>::iterator result = std::max_element(rewards.begin(),rewards.end());
        std::size_t dist = std::distance(rewards.begin(), result);

        action = legalMoves[dist];

        return action;
    }
};
