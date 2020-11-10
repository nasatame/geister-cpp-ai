
#include <vector>
#include <algorithm>
#include <iostream>
#include <random>
#include <iomanip>
#include <limits>
#include <cassert>
#include <fstream>

#include "random.hpp"
#include "player.hpp"
#include "random.hpp"
#include "geister.hpp"
#include "simulator.hpp"

#include "aiueo.hpp"

constexpr int PLAYOUT_COUNT = 100000;
constexpr int MAX_NODE = 10000;
constexpr int N_THR = 1000;
constexpr double alpha = 0.114142356;
constexpr bool DISPLAY_ON = true;

//maxを選ぶノードをmaxノードと呼称する。
//一番最初はmaxノードである。
//次のノードはminノードである。
//maxノードではturn_playerは、1である。
//minノードではturn_playerは、-1である。
//turn_playerは先攻後攻の別を表し1は先攻である。

//maxノードでは、gameは後攻の手が入力された状態であり、自分の手は入力されていない。
//よってmaxノードの自身の評価を行うためには、先攻の手から入力されるようにしなければならない。

//minノードでは、gameは先攻の手が入力された状態であり、自分の手は入力されていない。
//よってminノードの自身の評価を行うためには、後攻の手から入力されるようにしなければならない。

//get_ucb1は、maxノードでは、先攻が有利な程高い値を返す。よってこれが大きいノードであればあるほど先攻有利である。
//get_ucb1は、minノードでは、後攻が有利なほど高い値を返す。よってこれが大きいノードであればあるほど後攻が有利である。

//このことを理解して、select関数によるノードの選択では、childrenのノードの値を見ている。

//maxノードで手に入る値は、これを選べばどの程度後攻が有利かであり、先攻が有利であるか知りたいのに不適切である。

//よってget_ucb1に変更を行う。
//変更後の仕様は、
//get_ucb1_1stは、先攻が有利な程高い値を返す。よってこれが大きいノードであればあるほど先攻有利である。
//get_ucb1_2ndは、後攻が有利な程高い値を返す。よってこれが大きいノードであればあるほど先攻有利である。

class MonteCarloTree;

struct MonteCarloNode
{
    MonteCarloNode()
        //ダミー
        : leagalMove("AB11,N")
    {
        is_leaf = true;
    }

    //leafであるか？
    bool is_leaf;
    //シミュレーションに用いる情報
    Geister game;
    Hand leagalMove;
    //勝率情報
    int playout;         //全体的に自分がプレイアウトされた回数
    int playout_as_leaf; //葉としてプレイアウトされた回数。これは定数になると思う。
    double reward_sum;   //勝ち状態の合計、0や-になりうる

    //**このプログラムでは自分は必ず先攻として扱われる**
    //1:先攻（評価値は＋） -1:後攻（評価値はー）
    int turn_player;
    //node情報系
    int my_node_num; //自身のノード番号
    int parent;
    std::vector<int> children;

    //関数
    //ucb1を入手する。
    double get_ucb1_1st(const MonteCarloTree &tree) const;

    //後攻の時は、後攻が有利な手を選びたい。reward_sumが負のノードをなるべく選ぶようにするために、符号を反転して返す。
    double get_ucb1_2nd(const MonteCarloTree &tree) const;

    //ボード情報の出力
    std::string printBoard() const;
};

class MonteCarloTree
{
protected:
    cpprefjp::random_device rd;
    std::mt19937 mt;

public:
    //設定情報
    static constexpr int max_node = MAX_NODE;
    static constexpr int Nthr = N_THR;
    //ルートノードは0
    static std::vector<MonteCarloNode> nodes;
    int nodes_count;

    //現在の盤面情報を渡されて初期化する。
    //相手の色はplayoutごとにランダムとする。
    MonteCarloTree()
        : mt(std::mt19937(rd()))
    {
        //並列化の際には注意
        static bool init_flag = false;

        nodes.clear();
        nodes.resize(max_node);

        dummy();
    }

    void setPurpleColor(Geister &game) const;
    void decisiveColor(Geister &game) const;

    MonteCarloTree(Geister game) : MonteCarloTree()
    {
        //ここで相手の色を決め打ちしてしまう。
        //decisiveColor(game);
        setPurpleColor(game);

        init(game);
    }

    //一番最初のノードの初期化、
    //自身はmaxノードである。
    void init(Geister game)
    {
        //std::cerr << "MonteCalor : init" << std::endl;

        nodes_count = 0;
        //ターンプレイヤーの設定
        nodes.at(nodes_count).turn_player = 1;

        //TODO:元から展開するときはここに注意
        nodes.at(nodes_count).is_leaf = true;
        //シミュレーションに用いる情報
        nodes.at(nodes_count).game = game;

        //std::cerr << nodes.at(nodes_count).game << std::endl;

        //勝率情報
        nodes.at(nodes_count).playout = 0, nodes.at(nodes_count).playout_as_leaf = 0, nodes.at(nodes_count).reward_sum = 0;

        //node情報系
        nodes.at(nodes_count).my_node_num = nodes_count;
        nodes.at(nodes_count).parent = -1;
        nodes.at(nodes_count).children.clear();

        nodes_count = 1;

        //本来なら、ここで一段階展開するのが正しいのかもしれない。
        //expand(0);
    }

    //先攻の時も後攻の時もucb1が最大のノードを選択する。が使用する関数については異なるため注意が必要である。
    //turn_player == 1ならmaxノードであるため、後攻の手が入力された状態であり、なるべく先攻が有利なノードを選択する。
    int select(int node_num)
    {

        int run_i = -1;
        double ucb1 = -10000000;

        //std::cerr << "MonteCarlo : select " << node_num << ", " << nodes.at(node_num).children.size() << std::endl;

        for (int k = 0; k < nodes.at(node_num).children.size(); k++)
        {

            double child_ucb1;
            if (nodes.at(node_num).turn_player == 1)
            {
                //std::cerr << "MonteCarlo : select_child " << nodes.at(node_num).children[k]  << std::endl;
                child_ucb1 = nodes.at(nodes.at(node_num).children[k]).get_ucb1_1st(std::ref(*this));
            }
            else
            {
                child_ucb1 = nodes.at(nodes.at(node_num).children[k]).get_ucb1_2nd(std::ref(*this));
            }

            if (ucb1 < child_ucb1)
            {
                ucb1 = child_ucb1;
                run_i = k;
            }
        }

        if (run_i == -1)
        {
            std::cerr << "errer not leaf but no children" << std::endl;
            std::exit(-1);
        }
        //std::cerr << "MonteCarlo : select_next " << nodes.at(node_num).children[run_i] << std::endl;

        return nodes.at(node_num).children[run_i];
    }

    int expand(int parent)
    {
        //正常な手の列挙

        std::vector<Hand> legalMoves;
        //自身がmaxノードである。つまり親はminノードである。
        //親には先攻の動きが入力されている。だから2nd
        if (nodes.at(parent).turn_player == -1)
            legalMoves = nodes.at(parent).game.getLegalMove2nd();
        //自身がminノードである。つまり、親はmaxノードである。
        //親には後攻の動きが入力されている。だから1st
        else if (nodes.at(parent).turn_player == 1)
            legalMoves = nodes.at(parent).game.getLegalMove1st();
        else
        {
            std::cerr << "errro turn_player is errer state." << std::endl;
            std::exit(-1);
        }

        for (int i = 0; i < legalMoves.size(); i++)
        {
            if (nodes.size() - 5 <= nodes_count)
            {
                //現状だと落ちてしまうので改善、
                nodes.resize(nodes.size() * 2);
                //std::cerr << "error array is not enough." << std::endl;
                //std::exit(-1);
            }

            //ターンプレイヤーの設定
            nodes.at(nodes_count).turn_player = -1 * nodes.at(parent).turn_player;

            Geister next(nodes.at(parent).game);
            next.move(legalMoves[i]);

            nodes.at(nodes_count).is_leaf = true;
            //シミュレーションに用いる情報
            nodes.at(nodes_count).game = next;
            nodes.at(nodes_count).leagalMove = legalMoves[i];

            //勝率情報
            nodes.at(nodes_count).playout = 0, nodes.at(nodes_count).playout_as_leaf = 0, nodes.at(nodes_count).reward_sum = 0;

            //node情報系
            nodes.at(nodes_count).my_node_num = nodes_count;
            nodes.at(nodes_count).parent = parent;
            nodes.at(nodes_count).children.clear();

            nodes.at(parent).children.push_back(nodes_count);

            nodes_count++;
        }

        return legalMoves.size();
    }

    //不適格なプレイアウト関数を書き直す
    //TODO:相手の脱出が読めていない気がする。紫駒の挙動を確認。
    void playout(int node_num, Simulator &sim)
    {
        //std::cerr << "MonteCarlo : playout start" << std::endl;
        static std::uniform_int_distribution<> selector;

        //自分が先攻の時は自分の動きはもう入力されているので、相手の動きから始まる。
        //自身がmaxノードである。
        //後攻の手が入力された状態から始まる。
        if (nodes.at(node_num).turn_player == 1)
        {
            while (true)
            {
                //std::cerr << sim.current.toString() << std::endl;

                if (sim.current.isEnd())
                    break;
                std::vector<Hand> &lm1 = sim.current.getLegalMove1st();

                //ゴールにいたら必ず移動させるようにする。
                for (int i = 0; i < lm1.size(); i++)
                {
                    Hand &hand = lm1.at(i);
                    if (hand.unit.color() == UnitColor::Blue)
                    {
                        if (hand.direct == Direction::East && hand.unit.x() == 5)
                        {
                            sim.current.move(hand);
                            return;
                        }
                        else if (hand.direct == Direction::West && hand.unit.x() == 0)
                        {
                            sim.current.move(hand);
                            return;
                        }
                    }
                }

                selector.param(std::uniform_int_distribution<>::param_type(0, lm1.size() - 1));
                //std::cerr << "MonteCarlo : playout selector " << lm1.size() << std::endl;
                Hand &m1 = lm1.at(selector(mt));
                if (m1.direct == 'S' && selector(mt) % 2 == 0)
                {
                    m1 = lm1.at(selector(mt));
                }
                //std::cerr << "MonteCarlo : playout 1st move" << ", " << a << std::endl;
                sim.current.move(m1);
                //std::cerr << "MonteCarlo : playout middle" << std::endl;
                if (sim.current.isEnd())
                    break;
                std::vector<Hand> &lm2 = sim.current.getLegalMove2nd();

                //ゴールにいたら必ず移動させるようにする。
                for (int i = 0; i < lm2.size(); i++)
                {
                    Hand &hand = lm2.at(i);
                    if (hand.unit.color() == UnitColor::blue)
                    {
                        if (hand.direct == Direction::East && hand.unit.x() == 5)
                        {
                            sim.current.move(hand);
                            return;
                        }
                        else if (hand.direct == Direction::West && hand.unit.x() == 0)
                        {
                            sim.current.move(hand);
                            return;
                        }
                    }
                }

                selector.param(std::uniform_int_distribution<>::param_type(0, lm2.size() - 1));
                Hand &m2 = lm2.at(selector(mt));
                if (m2.direct == 'W' && selector(mt) % 2 == 0)
                {
                    m2 = lm2.at(selector(mt));
                }
                //std::cerr << "MonteCarlo : playout 2nd move" << std::endl;
                sim.current.move(m2);
            }
            //std::cerr << "MonteCarlo : playout end " << sim.current.isEnd() << std::endl;
            return;
        }

        //自身がminノードである。
        //先攻の手が入力された状態から始まる。
        if (nodes.at(node_num).turn_player == -1)
        {
            while (true)
            {
                if (sim.current.isEnd())
                    break;
                std::vector<Hand> &lm2 = sim.current.getLegalMove2nd();

                //ゴールにいたら必ず移動させるようにする。
                for (int i = 0; i < lm2.size(); i++)
                {
                    Hand &hand = lm2.at(i);
                    if (hand.unit.color() == UnitColor::blue)
                    {
                        if (hand.direct == Direction::East && hand.unit.x() == 5)
                        {
                            sim.current.move(hand);
                            return;
                        }
                        else if (hand.direct == Direction::West && hand.unit.x() == 0)
                        {
                            sim.current.move(hand);
                            return;
                        }
                    }
                }

                selector.param(std::uniform_int_distribution<>::param_type(0, lm2.size() - 1));
                Hand &m2 = lm2.at(selector(mt));
                if (m2.direct == 'W' && selector(mt) % 2 == 0)
                {
                    m2 = lm2.at(selector(mt));
                }
                sim.current.move(m2);
                if (sim.current.isEnd())
                    break;
                std::vector<Hand> &lm1 = sim.current.getLegalMove1st();

                //ゴールにいたら必ず移動させるようにする。
                for (int i = 0; i < lm1.size(); i++)
                {
                    Hand &hand = lm1.at(i);
                    if (hand.unit.color() == UnitColor::Blue)
                    {
                        if (hand.direct == Direction::East && hand.unit.x() == 5)
                        {
                            sim.current.move(hand);
                            return;
                        }
                        else if (hand.direct == Direction::West && hand.unit.x() == 0)
                        {
                            sim.current.move(hand);
                            return;
                        }
                    }
                }

                selector.param(std::uniform_int_distribution<>::param_type(0, lm1.size() - 1));
                Hand &m1 = lm1.at(selector(mt));
                if (m1.direct == 'S' && selector(mt) % 2 == 0)
                {
                    m1 = lm1.at(selector(mt));
                }
                sim.current.move(m1);
            }

            //std::cerr << "MonteCarlo : playout end" << std::endl;
            return;
        }

        {
            std::cerr << "errer playout process is no way." << std::endl;
            std::exit(-1);
            return;
        }
    }

    //先攻勝ち:1 引き分け:0　後攻勝ち:-1
    double evaluate(int node_num)
    {
        //ここはそのまま使いたい。

        //とりあえずは一回だけプレイアウトするようにする。
        Simulator sim(nodes.at(node_num).game);

        sim.current = sim.root;

        //相手の色を決める。
        //ここでは、毎度毎度色が変わるせいで食い違いが起こるため、init時に変更する。
        //decisiveColor(sim.current);

        //実際にプレイアウトして試す。
        //std::cerr << "evaluate : " << node_num << ", " << sim.current << std::endl;
        //sim.setColor(sim.getRandomPattern());
        playout(node_num, std::ref(sim));

        return sim.evaluate();
    }

    //更新処理を行う。
    void update(int node_num, int eval, bool is_leaf)
    {
        nodes.at(node_num).playout += 1;
        nodes.at(node_num).reward_sum += eval;
        if (is_leaf)
            nodes.at(node_num).playout_as_leaf += 1;
        return;
    }

    //メインの部分
    //先攻勝ち:1 引き分け:0　後攻勝ち:-1
    int search(int node_num = 0)
    {
        //std::cerr << "MonteCarlo : search " << node_num << std::endl;

        //再帰を使って書くため呼び出しはあちらがわ
        if (nodes.at(node_num).is_leaf == true)
        {
            //yes

            //std::cerr << "MonteCarlo : search playout " << nodes.at(node_num).playout << std::endl;
            if (nodes.at(node_num).playout >= Nthr)
            {
                //展開する
                int child_count = expand(node_num);
                //展開できなかった。
                if (child_count == 0)
                {
                }
                else
                {
                    //葉ではなくなった。
                    nodes.at(node_num).is_leaf = false;
                }
                //またリーフノードまで移動。
            }

            if (nodes.at(node_num).is_leaf == true)
            {
                //自身がリーフノードの時、evaluate関数により評価する
                int eval = evaluate(node_num);
                update(node_num, eval, true);
                //std::cerr << "MonteCarlo : search is_leaf " << node_num << ", " << eval << std::endl;
                return eval;
            }
        }

        if (nodes.at(node_num).is_leaf == false)
        {
            //no
            //リーフノードまで移動

            //選択処理を実行する
            int next_node_num = select(node_num);

            //1手進める。
            int eval = search(next_node_num);
            //評価が返ってきた。更新ステップ
            update(node_num, eval, false);

            return eval;
        }

        {
            std::cerr << "errer process is no way." << std::endl;
            std::exit(-1);
        }
    }

    void displayDOTLaunguage(int node_num, std::ofstream& ofs)
    {

        double eval = -1;
        //現在の手番が先攻なら、後攻がどのくらい有利であるか
        if (nodes.at(node_num).turn_player == -1)
        {
            eval = nodes.at(node_num).get_ucb1_1st(*this);
        }
        else
        {
            eval = nodes.at(node_num).get_ucb1_2nd(*this);
        }
        //n002 [label="+"] ;
        //std::cerr << std::format("n{0:03} [label=\"{1:.3}\"] ;",node_num+2,eval) << std::endl;
        std::string board = nodes.at(node_num).printBoard();
        ofs << "n" << std::setfill('0') << std::setw(3) << node_num+2 << " [shape=box , fontname=\"monospace\" , label=\"" << std::setprecision(3) << eval << "\\n" << board <<  "\"] ;"  << std::endl;

        for (int i = 0; i < nodes.at(node_num).children.size(); i++)
        {
            //std::cerr << node_num << " " << nodes.at(node_num).children[i] << " ";
            //n002 -- n003 ;
            //std::cerr << std::format("n{0:03} -- n{1:03} ;",node_num+2,nodes.at(node_num).children[i]+2) << std::endl;
            ofs << "n" <<  std::setfill('0') << std::setw(3) << node_num+2 << " -- n" <<  std::setfill('0') << std::setw(3) << nodes.at(node_num).children[i]+2 << " ;" << std::endl;

            int child_num = nodes.at(node_num).children[i];
            if (nodes.at(node_num).turn_player == 1)
            {
                //std::cerr << std::setprecision(3) << nodes.at(child_num).get_ucb1_1st(*this) << std::endl;
                //std::cerr << nodes.at(child_num).playout << std::endl;
            }
            else
            {
                //std::cerr << std::setprecision(3) << nodes.at(child_num).get_ucb1_2nd(*this) << std::endl;
                //std::cerr << nodes.at(child_num).playout << std::endl;
            }

            displayDOTLaunguage(child_num,std::ref(ofs));
        }
    }

    void displayDOTLaunguage()
    {
        if (DISPLAY_ON)
        {
            std::ofstream ofs("thisfile");

            ofs << "graph \"\" " << std::endl;
            ofs << "{" << std::endl;
            ofs << "label=\"game tree graph\" " << std::endl;
            ofs << "subgraph gametree " << std::endl;
            ofs << "{" << std::endl;
            ofs << "label=\"game tree graph\" " << std::endl;

            ofs << "n002 ; " << std::endl;
            displayDOTLaunguage(0,std::ref(ofs));

            ofs << "}" << std::endl;
            ofs << "}" << std::endl;
        }
    }

    void display(int node_num)
    {

        for (int i = 0; i < nodes.at(node_num).children.size(); i++)
        {
            std::cerr << node_num << " " << nodes.at(node_num).children[i] << " ";

            int child_num = nodes.at(node_num).children[i];
            if (nodes.at(node_num).turn_player == 1)
            {
                std::cerr << std::setprecision(3) << nodes.at(child_num).get_ucb1_1st(*this) << std::endl;
                //std::cerr << nodes.at(child_num).playout << std::endl;
            }
            else
            {
                std::cerr << std::setprecision(3) << nodes.at(child_num).get_ucb1_2nd(*this) << std::endl;
                //std::cerr << nodes.at(child_num).playout << std::endl;
            }

            display(child_num);
        }
    }

    //ゲーム木を表示するデバック用の関数
    void display()
    {
        if (DISPLAY_ON)
        {

            std::cerr << "display" << std::endl;

            std::cerr << nodes_count << " " << nodes_count - 1 << std::endl;

            display(0);
        }
    }
};

std::vector<MonteCarloNode> MonteCarloTree::nodes;

class MonteCarloPlayer : public Player
{
    uint32_t playoutCount = PLAYOUT_COUNT;
    cpprefjp::random_device rd;
    std::mt19937 mt;

public:
    MonteCarloPlayer() : mt(rd())
    {
    }

    virtual std::string decideRed()
    {
        std::uniform_int_distribution<int> serector(0, pattern.size() - 1);
        std::string color = pattern[serector(mt)];

        game.setColor(color, "");
        return color;
    }

    //現在のノード番号を展開する。もしくは、最も評価が高いノードに移動する。
    //展開した場合はplayoutして得られた評価を返す。
    std::string monteCarloTreeSearch(Geister game)
    {
        MonteCarloTree tree(game);

        int i = 0;
        for (i = 0; i < playoutCount; i++)
        {
            //std::cerr << "MonteCarlo : play " << i << std::endl;
            tree.search();
        }
        //std::cerr << "MonteCarlo : play end " << i << std::endl;

        tree.displayDOTLaunguage();
        //std::cerr << "MonteCarlo val : " << tree.nodes.at(tree.select(0)).get_ucb1_1st(tree) << std::endl;
        return tree.nodes.at(tree.select(0)).leagalMove;
    }

    virtual std::string decideHand(std::string res)
    {
        game.setState(res);
        //std::cerr << "MonteCarlo : Call " << game << std::endl;

        //std::cerr << "MonteCarlo : Search " << std::endl;
        Hand action = monteCarloTreeSearch(game);

        //std::cerr << "MonteCarlo : " << action.toString() << std::endl;

        return action;
    }
};

double MonteCarloNode::get_ucb1_1st(const MonteCarloTree &tree) const
{
    //UCB1 = μi + α√((log n) / ni)
    //μi:手iの報酬期待値
    //手iの報酬期待値の合計 = reward_sum
    //よって reward_sum / playout = μi
    //alpha:調整様パラメータ√2がいいらしい
    //n:兄弟ノードを含めたプレイアウト数 = t
    //ni:手iのプレイアウト回数 = playout

    //それはrootノードです。
    if (parent == -1)
        return 0;
    int t = tree.nodes.at(parent).playout - tree.nodes.at(parent).playout_as_leaf; //兄弟ノード全体のプレイアウト数
    if (playout == 0)
    {
        return std::numeric_limits<double>::infinity();
    }
    return reward_sum / (playout * 2) + 0.5 + alpha * std::sqrt(std::log(t) / playout);
}

double MonteCarloNode::get_ucb1_2nd(const MonteCarloTree &tree) const
{
    //それはrootノードです。
    if (parent == -1)
        return 0;
    int t = tree.nodes.at(parent).playout - tree.nodes.at(parent).playout_as_leaf; //兄弟ノード全体のプレイアウト数
    if (playout == 0)
    {
        return std::numeric_limits<double>::infinity();
    }
    return (-1 * reward_sum) / (playout * 2) + 0.5 + alpha * std::sqrt(std::log(t) / playout);
}

std::string MonteCarloNode::printBoard() const {

    const std::array<Unit, 16>& units =  this->game.allUnit();
    std::string info = "";
    info +=  "2ndPlayer Take: ";
    for(int i = 0; i < 8; ++i){
        const Unit& u = units[i];
        if(u.isTaken()){
            /*
            if(u.isBlue() && u.isRed())
                info +=  "\\e[35m";
            else if(u.isBlue())
                info +=  "\\e[34m";
            else if(u.isRed())
                info +=  "\\e[31m";
            */
            info +=  u.name();
            //info +=  "\\e[0m";
            info += ",";
        }
    }
    info +=  "\\l";
    info +=  std::string("  0 1 2 3 4 5") + "\\l";
    for(int i = 0; i < 6; ++i){
        info +=  std::to_string(i);
        for(int j = 0; j < 6; ++j){
            info +=  " ";
            bool exist = false;
            for(const Unit& u: units){
                if(u.x() == j && u.y() == i){
                    /*
                    if(u.isBlue() && u.isRed())
                        info +=  "\\e[35m";
                    else if(u.isBlue())
                        info +=  "\\e[34m";
                    else if(u.isRed())
                        info +=  "\\e[31m";
                    */
                    info +=  u.name();
                    //info +=  "\\e[0m";
                    exist = true;
                    break;
                }
            }
            if(!exist) info +=  " ";
        }
        info +=  "\\l";
    }
    info +=  "1stPlayer Take: ";
    for(int i = 8; i < 16; ++i){
        const Unit& u = units[i];
        if(u.isTaken()){
            /*
            if(u.isBlue() && u.isRed())
                info +=  "\\e[35m";
            else if(u.isBlue())
                info +=  "\\e[34m";
            else if(u.isRed())
                info +=  "\\e[31m";
            */
            info +=  u.name();
            //info +=  std::string("\\e[0m") + ",";
            info += ",";
        }
    }
    info +=  "\\l";
    for(int i = 0; i < 16; ++i){
        const Unit& u = units[i];
        if(u.isEscape()){
            //info +=  std::string("Escape: ") + "\\e[34m" + u.name() + "\\e[0m" + "\\l";
            info +=  std::string("Escape: ") + u.name() + "\\l";
        }
    }
    info +=  "\\l";

    return info;
}

void MonteCarloTree::decisiveColor(Geister &game) const
{
    std::array<Unit, 16> units_all = game.allUnit();
    std::vector<Unit> units;
    int count = 0;
    for (int i = 0; i < 16; i++)
    {
        if (units_all[i].is2nd() && !units_all[i].isTaken())
        {
            units.push_back(units_all[i]);
        }
        count += units_all[i].is2nd();
        assert(!units_all[i].isEscape());
    }

    assert(count == 8);

    //相手のゴールに近い駒を青とする。

    //std::cerr << "MonteCarlo : SetColor" << std::endl;
    //後攻の駒色を決める。
    int left_blue_2nd = 4 - game.takenCount(UnitColor::blue);
    int left_red_2nd = 4 - game.takenCount(UnitColor::red);
    //ゴールまでの距離の昇順
    std::sort(units.begin(), units.end(), [](const Unit &left, const Unit &right) {
        int l05 = abs(left.x() - 0) + abs(left.y() - 5);
        int l55 = abs(left.x() - 5) + abs(left.y() - 5);
        int r05 = abs(right.x() - 0) + abs(right.y() - 5);
        int r55 = abs(right.x() - 5) + abs(right.y() - 5);

        return std::min(l05, l55) < std::min(r05, r55);
    });

    //assert(units.size() == left_blue_2nd + left_red_2nd);
    if (units.size() != left_blue_2nd + left_red_2nd)
    {
        std::cerr << left_blue_2nd << " " << left_red_2nd << std::endl;
        std::cerr << game.takenCount(UnitColor::blue) << " " << game.takenCount(UnitColor::red) << std::endl;
        std::cerr << game.takenCount(UnitColor::Blue) << " " << game.takenCount(UnitColor::Red) << std::endl;
        std::cerr << units.size() << std::endl;
        std::cerr << "enemy color can't desiced." << std::endl;
        exit(-1);
    }

    //ここまでが現存している駒
    for (int i = 0; i < units.size(); i++)
    {

        if (i < left_blue_2nd)
            game.setColor(units[i].name(), UnitColor::blue);
        else
            game.setColor(units[i].name(), UnitColor::red);
    }
    //ここからは盤面にない駒に対して適当に塗っていく
    for (int i = 0; i < 16; i++)
    {
        if (units_all[i].is2nd() && units_all[i].isTaken())
        {
            if (left_blue_2nd < 4)
            {
                game.setColor(units_all[i].name(), UnitColor::blue);
                left_blue_2nd++;
            }
            else if (left_red_2nd < 4)
            {
                game.setColor(units_all[i].name(), UnitColor::red);
                left_red_2nd++;
            }
        }
    }
}

void MonteCarloTree::setPurpleColor(Geister &game) const
{
    std::array<Unit, 16> units_all = game.allUnit();
    int count = 0;
    for (int i = 0; i < 16; i++)
    {
        if (units_all[i].is2nd() && !units_all[i].isTaken())
        {
            game.setColor(units_all[i].name(), UnitColor::purple);
        }
        count += units_all[i].is2nd() ? 1 : 0;
    }

    if (false && count != 8)
    {
        int left_blue_2nd = 4 - game.takenCount(UnitColor::blue);
        int left_red_2nd = 4 - game.takenCount(UnitColor::red);
        std::cerr << left_blue_2nd << " " << left_red_2nd << std::endl;
        std::cerr << game.takenCount(UnitColor::blue) << " " << game.takenCount(UnitColor::red) << std::endl;
        std::cerr << game.takenCount(UnitColor::Blue) << " " << game.takenCount(UnitColor::Red) << std::endl;
        std::cerr << count << std::endl;
        std::cerr << "enemy color can't desiced." << std::endl;
        //exit(-1);
    }
}
