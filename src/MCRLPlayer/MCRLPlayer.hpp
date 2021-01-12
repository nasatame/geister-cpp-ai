
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

constexpr int PLAYOUT_COUNT = 100000;
constexpr int MAX_NODE = 10000;
constexpr int N_THR = 1000;
constexpr double alpha = 0.114142356;
constexpr bool DISPLAY_ON = true;

//max��I�ԃm�[�h��max�m�[�h�ƌď̂���B
//��ԍŏ���max�m�[�h�ł���B
//���̃m�[�h��min�m�[�h�ł���B
//max�m�[�h�ł�turn_player�́A1�ł���B
//min�m�[�h�ł�turn_player�́A-1�ł���B
//turn_player�͐�U��U�̕ʂ�\��1�͐�U�ł���B

//max�m�[�h�ł́Agame�͌�U�̎肪���͂��ꂽ��Ԃł���A�����̎�͓��͂���Ă��Ȃ��B
//�����max�m�[�h�̎��g�̕]�����s�����߂ɂ́A��U�̎肩����͂����悤�ɂ��Ȃ���΂Ȃ�Ȃ��B

//min�m�[�h�ł́Agame�͐�U�̎肪���͂��ꂽ��Ԃł���A�����̎�͓��͂���Ă��Ȃ��B
//�����min�m�[�h�̎��g�̕]�����s�����߂ɂ́A��U�̎肩����͂����悤�ɂ��Ȃ���΂Ȃ�Ȃ��B

//get_ucb1�́Amax�m�[�h�ł́A��U���L���Ȓ������l��Ԃ��B����Ă��ꂪ�傫���m�[�h�ł���΂���قǐ�U�L���ł���B
//get_ucb1�́Amin�m�[�h�ł́A��U���L���ȂقǍ����l��Ԃ��B����Ă��ꂪ�傫���m�[�h�ł���΂���قǌ�U���L���ł���B

//���̂��Ƃ𗝉����āAselect�֐��ɂ��m�[�h�̑I���ł́Achildren�̃m�[�h�̒l�����Ă���B

//max�m�[�h�Ŏ�ɓ���l�́A�����I�ׂ΂ǂ̒��x��U���L�����ł���A��U���L���ł��邩�m�肽���̂ɕs�K�؂ł���B

//�����get_ucb1�ɕύX���s���B
//�ύX��̎d�l�́A
//get_ucb1_1st�́A��U���L���Ȓ������l��Ԃ��B����Ă��ꂪ�傫���m�[�h�ł���΂���قǐ�U�L���ł���B
//get_ucb1_2nd�́A��U���L���Ȓ������l��Ԃ��B����Ă��ꂪ�傫���m�[�h�ł���΂���قǐ�U�L���ł���B

class MonteCarloTree;

struct MonteCarloNode
{
    MonteCarloNode()
        //�_�~�[
        : leagalMove("AB11,N")
    {
        is_leaf = true;
    }

    //leaf�ł��邩�H
    bool is_leaf;
    //�V�~�����[�V�����ɗp������
    Geister game;
    Hand leagalMove;
    //�������
    int playout;         //�S�̓I�Ɏ������v���C�A�E�g���ꂽ��
    int playout_as_leaf; //�t�Ƃ��ăv���C�A�E�g���ꂽ�񐔁B����͒萔�ɂȂ�Ǝv���B
    double reward_sum;   //������Ԃ̍��v�A0��-�ɂȂ肤��

    //**���̃v���O�����ł͎����͕K����U�Ƃ��Ĉ�����**
    //1:��U�i�]���l�́{�j -1:��U�i�]���l�́[�j
    int turn_player;
    //node���n
    int my_node_num; //���g�̃m�[�h�ԍ�
    int parent;
    std::vector<int> children;

    //�֐�
    //ucb1����肷��B
    double get_ucb1_1st(const MonteCarloTree &tree) const;

    //��U�̎��́A��U���L���Ȏ��I�т����Breward_sum�����̃m�[�h���Ȃ�ׂ��I�Ԃ悤�ɂ��邽�߂ɁA�����𔽓]���ĕԂ��B
    double get_ucb1_2nd(const MonteCarloTree &tree) const;

    //�{�[�h���̏o��
    std::string printBoard() const;
};

class MonteCarloTree
{
protected:
    cpprefjp::random_device rd;
    std::mt19937 mt;
public:
    //�ݒ���
    static constexpr int max_node = MAX_NODE;
    static constexpr int Nthr = N_THR;
    //���[�g�m�[�h��0
    static std::vector<MonteCarloNode> nodes;
    int nodes_count;
    int turn;

    //���݂̔Ֆʏ���n����ď���������B
    //����̐F��playout���ƂɃ����_���Ƃ���B
    MonteCarloTree()
        : mt(std::mt19937(rd()))
    {
        //���񉻂̍ۂɂ͒���
        static bool init_flag = false;

        nodes.clear();
        nodes.resize(max_node);

        dummy();
    }

    void setPurpleColor(Geister &game) const;
    void decisiveColor(Geister &game) const;

    MonteCarloTree(Geister game) : MonteCarloTree()
    {
        //�����ő���̐F�����ߑł����Ă��܂��B
        //decisiveColor(game);
        setPurpleColor(game);

        init(game);
    }

    //��ԍŏ��̃m�[�h�̏������A
    //���g��max�m�[�h�ł���B
    void init(Geister game)
    {
        //std::cerr << "MonteCalor : init" << std::endl;

        nodes_count = 0;
        //�^�[���v���C���[�̐ݒ�
        nodes.at(nodes_count).turn_player = 1;

        //TODO:������W�J����Ƃ��͂����ɒ���
        nodes.at(nodes_count).is_leaf = true;
        //�V�~�����[�V�����ɗp������
        nodes.at(nodes_count).game = game;

        //�������
        nodes.at(nodes_count).playout = 0, nodes.at(nodes_count).playout_as_leaf = 0, nodes.at(nodes_count).reward_sum = 0;

        //node���n
        nodes.at(nodes_count).my_node_num = nodes_count;
        nodes.at(nodes_count).parent = -1;
        nodes.at(nodes_count).children.clear();

        nodes_count = 1;

        //�{���Ȃ�A�����ň�i�K�W�J����̂��������̂�������Ȃ��B
        //expand(0);
    }

    //��U�̎�����U�̎���ucb1���ő�̃m�[�h��I������B���g�p����֐��ɂ��Ă͈قȂ邽�ߒ��ӂ��K�v�ł���B
    //turn_player == 1�Ȃ�max�m�[�h�ł��邽�߁A��U�̎肪���͂��ꂽ��Ԃł���A�Ȃ�ׂ���U���L���ȃm�[�h��I������B
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
        //����Ȏ�̗�

        std::vector<Hand> legalMoves;
        //���g��max�m�[�h�ł���B�܂�e��min�m�[�h�ł���B
        //�e�ɂ͐�U�̓��������͂���Ă���B������2nd
        if( nodes.at(parent).game.result() != Result::OnPlay ){
            std::cerr << "errro OnPlay." << nodes.at(parent).game.result() << std::endl;
            return legalMoves.size();
        }

        if (nodes.at(parent).turn_player == -1)
            legalMoves = nodes.at(parent).game.getLegalMove2nd();
        //���g��min�m�[�h�ł���B�܂�A�e��max�m�[�h�ł���B
        //�e�ɂ͌�U�̓��������͂���Ă���B������1st
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
                //�����Ă��܂��̂ŉ��P�A
                nodes.resize(nodes.size() * 2);
                //std::cerr << "error array is not enough." << std::endl;
                //std::exit(-1);
            }

            //�^�[���v���C���[�̐ݒ�
            nodes.at(nodes_count).turn_player = -1 * nodes.at(parent).turn_player;

            Geister next(nodes.at(parent).game);
            next.move(legalMoves[i]);

            nodes.at(nodes_count).is_leaf = true;
            //�V�~�����[�V�����ɗp������
            nodes.at(nodes_count).game = next;
            nodes.at(nodes_count).leagalMove = legalMoves[i];

            //�������
            nodes.at(nodes_count).playout = 0, nodes.at(nodes_count).playout_as_leaf = 0, nodes.at(nodes_count).reward_sum = 0;

            //node���n
            nodes.at(nodes_count).my_node_num = nodes_count;
            nodes.at(nodes_count).parent = parent;
            nodes.at(nodes_count).children.clear();

            nodes.at(parent).children.push_back(nodes_count);

            nodes_count++;
        }

        return legalMoves.size();
    }

    //�s�K�i�ȃv���C�A�E�g�֐�����������
    //TODO:����̒E�o���ǂ߂Ă��Ȃ��C������B����̋������m�F�B
    void playout(int node_num, Simulator &sim)
    {
        //std::cerr << "MonteCarlo : playout start" << std::endl;
        static std::uniform_int_distribution<> selector;

        //��������U�̎��͎����̓����͂������͂���Ă���̂ŁA����̓�������n�܂�B
        //���g��max�m�[�h�ł���B
        //��U�̎肪���͂��ꂽ��Ԃ���n�܂�B
        if (nodes.at(node_num).turn_player == 1)
        {
            while (true)
            {
                //std::cerr << sim.current.toString() << std::endl;

                if (sim.current.isEnd())
                    break;
                std::vector<Hand> &lm1 = sim.current.getLegalMove1st();

                //�S�[���ɂ�����K���ړ�������悤�ɂ���B
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

                //�S�[���ɂ�����K���ړ�������悤�ɂ���B
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

        //���g��min�m�[�h�ł���B
        //��U�̎肪���͂��ꂽ��Ԃ���n�܂�B
        if (nodes.at(node_num).turn_player == -1)
        {
            while (true)
            {
                if (sim.current.isEnd())
                    break;
                std::vector<Hand> &lm2 = sim.current.getLegalMove2nd();

                //�S�[���ɂ�����K���ړ�������悤�ɂ���B
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

                //�S�[���ɂ�����K���ړ�������悤�ɂ���B
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

    //��U����:1 ��������:0�@��U����:-1
    double evaluate(int node_num)
    {
        //�����͂��̂܂܎g�������B

        //�Ƃ肠�����͈�񂾂��v���C�A�E�g����悤�ɂ���B
        Simulator sim(nodes.at(node_num).game);

        sim.current = sim.root;

        //����̐F�����߂�B
        //�����ł́A���x���x�F���ς�邹���ŐH���Ⴂ���N���邽�߁Ainit���ɕύX����B
        //decisiveColor(sim.current);

        //���ۂɃv���C�A�E�g���Ď����B
        //std::cerr << "evaluate : " << node_num << ", " << sim.current << std::endl;
        //sim.setColor(sim.getRandomPattern());
        playout(node_num, std::ref(sim));

        return sim.evaluate();
    }

    //�X�V�������s���B
    void update(int node_num, int eval, bool is_leaf)
    {
        nodes.at(node_num).playout += 1;
        nodes.at(node_num).reward_sum += eval;
        if (is_leaf)
            nodes.at(node_num).playout_as_leaf += 1;
        return;
    }

    //���C���̕���
    //��U����:1 ��������:0�@��U����:-1
    int search(int node_num = 0)
    {
        //std::cerr << "MonteCarlo : search " << node_num << std::endl;

        //�ċA���g���ď������ߌĂяo���͂����炪��
        if (nodes.at(node_num).is_leaf == true)
        {
            //yes

            //std::cerr << "MonteCarlo : search playout " << nodes.at(node_num).playout << std::endl;
            if (nodes.at(node_num).playout >= Nthr)
            {
                //�W�J����
                int child_count = expand(node_num);
                //�W�J�ł��Ȃ������B
                if (child_count == 0)
                {
                }
                else
                {
                    //�t�ł͂Ȃ��Ȃ����B
                    nodes.at(node_num).is_leaf = false;
                }
                //�܂����[�t�m�[�h�܂ňړ��B
            }

            if (nodes.at(node_num).is_leaf == true)
            {
                //���g�����[�t�m�[�h�̎��Aevaluate�֐��ɂ��]������
                int eval = evaluate(node_num);
                update(node_num, eval, true);
                //std::cerr << "MonteCarlo : search is_leaf " << node_num << ", " << eval << std::endl;
                return eval;
            }
        }

        if (nodes.at(node_num).is_leaf == false)
        {
            //no
            //���[�t�m�[�h�܂ňړ�

            //�I�����������s����
            int next_node_num = select(node_num);

            //1��i�߂�B
            int eval = search(next_node_num);
            //�]�����Ԃ��Ă����B�X�V�X�e�b�v
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
        //���݂̎�Ԃ���U�Ȃ�A��U���ǂ̂��炢�L���ł��邩
        if (nodes.at(node_num).turn_player == -1)
        {
            eval = nodes.at(node_num).get_ucb1_1st(*this);
        }
        else
        {
            eval = nodes.at(node_num).get_ucb1_2nd(*this);
        }
        //n002 [label="+"];
        //std::cerr << std::format("n{0:03} [label=\"{1:.3}\"] ;",node_num+2,eval) << std::endl;
        std::string board = nodes.at(node_num).printBoard();
        ofs << "n" << std::setfill('0') << std::setw(3) << node_num+2 << " [shape=box , fontname=\"monospace\" , label=\"" << std::setprecision(3) << eval << "," << nodes.at(node_num).playout << "\\n" << board <<  "\"] ;"  << std::endl;

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
            std::ofstream ofs("thisfile" + std::to_string(turn));

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

    //�Q�[���؂�\������f�o�b�N�p�̊֐�
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
protected:
    uint32_t playoutCount = PLAYOUT_COUNT;
    cpprefjp::random_device rd;
    std::mt19937 mt;
    int turn = 0;
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

    //���݂̃m�[�h�ԍ���W�J����B�������́A�ł��]���������m�[�h�Ɉړ�����B
    //�W�J�����ꍇ��playout���ē���ꂽ�]����Ԃ��B
    std::string monteCarloTreeSearch(Geister game)
    {
        MonteCarloTree tree(game);
        tree.turn = turn;
        std::cerr << "MonteCarlo : play " << turn << std::endl;
        
        int i = 0;
        for (i = 0; i < playoutCount; i++)
        {
            tree.search();
        }
        //std::cerr << "MonteCarlo : play end " << i << std::endl;

        tree.displayDOTLaunguage();
        //std::cerr << "MonteCarlo val : " << tree.nodes.at(tree.select(0)).get_ucb1_1st(tree) << std::endl;
        return tree.nodes.at(tree.select(0)).leagalMove;
    }

    virtual std::string decideHand(std::string res)
    {
        turn++;
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
    //UCB1 = ��i + ����((log n) / ni)
    //��i:��i�̕�V���Ғl
    //��i�̕�V���Ғl�̍��v = reward_sum
    //����� reward_sum / playout = ��i
    //alpha:�����l�p�����[�^��2�������炵��
    //n:�Z��m�[�h���܂߂��v���C�A�E�g�� = t
    //ni:��i�̃v���C�A�E�g�� = playout

    //�����root�m�[�h�ł��B
    if (parent == -1)
        return 0;
    int t = tree.nodes.at(parent).playout - tree.nodes.at(parent).playout_as_leaf; //�Z��m�[�h�S�̂̃v���C�A�E�g��
    if (playout == 0)
    {
        return std::numeric_limits<double>::infinity();
    }
    return reward_sum / (playout * 2) + 0.5 + alpha * std::sqrt(std::log(t) / playout);
}

double MonteCarloNode::get_ucb1_2nd(const MonteCarloTree &tree) const
{
    //�����root�m�[�h�ł��B
    if (parent == -1)
        return 0;
    int t = tree.nodes.at(parent).playout - tree.nodes.at(parent).playout_as_leaf; //�Z��m�[�h�S�̂̃v���C�A�E�g��
    if (playout == 0)
    {
        return std::numeric_limits<double>::infinity();
    }
    return (-1 * reward_sum) / (playout * 2) + 0.5 + alpha * std::sqrt(std::log(t) / playout);
}

std::string MonteCarloNode::printBoard() const {

    const std::array<Unit, 16>& units =  this->game.allUnit();
    std::string info = "";
    info +=  "2nd Take: ";
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
    info +=  "1st Take: ";
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

    //����̃S�[���ɋ߂����Ƃ���B

    //std::cerr << "MonteCarlo : SetColor" << std::endl;
    //��U�̋�F�����߂�B
    int left_blue_2nd = 4 - game.takenCount(UnitColor::blue);
    int left_red_2nd = 4 - game.takenCount(UnitColor::red);
    //�S�[���܂ł̋����̏���
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

    //�����܂ł��������Ă����
    for (int i = 0; i < units.size(); i++)
    {

        if (i < left_blue_2nd)
            game.setColor(units[i].name(), UnitColor::blue);
        else
            game.setColor(units[i].name(), UnitColor::red);
    }
    //��������͔ՖʂɂȂ���ɑ΂��ēK���ɓh���Ă���
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