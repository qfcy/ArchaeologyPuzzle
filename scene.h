#pragma once
#include <iostream>
#include <vector>
#include <list>
#include <set>
#include <unordered_map>
#include <queue>
#include <utility>
#include <stdexcept>
#include <climits>
#include <cmath>
#include <type_traits>

namespace _scene_h{
using namespace std;
// 定义迷宫元素
enum class Cell {
    empty = '_',  // 空
    wall = 'W',  // 墙
    wall2 = 'L', // 另一种墙壁
    pillar = 'P', // 柱子
    reward = 'R',  // 奖励物品
    start = 'S',  // 起点
    end = 'D',   // 终点
    auto_wall = 'w', // 机关装置，玩家经过之后会变成真正的墙
    trap = 'a', // 陷阱装置，玩家停留时会受到伤害
    teleport = 't', // 随机传送装置
    regen = 'r', // 生命恢复装置
    rocket = 'o', // 火箭物品（2个）
    boss = 'b', // Boss
    reward2 = '2', // 其他古代遗器
    reward3 = '3',
    reward4 = '4',
};
template<typename T>
typename enable_if<is_integral<T>::value,bool>::type
operator==(const Cell &cell,const T& other){
    return static_cast<T>(cell)==other;
}
template<typename T>
typename enable_if<is_integral<T>::value,bool>::type
operator==(const T& other,const Cell &cell){
    return static_cast<T>(cell)==other;
}
template<typename T>
typename enable_if<is_integral<T>::value,bool>::type
operator!=(const Cell &cell,const T& other){
    return static_cast<T>(cell)!=other;
}
template<typename T>
typename enable_if<is_integral<T>::value,bool>::type
operator!=(const T& other,const Cell &cell){
    return static_cast<T>(cell)!=other;
}
inline bool isWall(Cell block){
    if(block==Cell::wall) return true;
    if(block==Cell::wall2) return true;
    return false;
}
inline bool isSolid(Cell block){
    if(block==Cell::pillar) return true;
    return isWall(block);
}

struct Pos{ // y, x分别表示行、列
    int x;int y;
    Pos():x(0),y(0){};
    Pos(int y,int x):x(x),y(y){};
    inline bool operator<(const Pos& other) const {
        if (y == other.y) return x < other.x;
        return y < other.y;
    }
    inline bool operator>(const Pos& other) const {
        if (y == other.y) return x > other.x;
        return y > other.y;
    }
    inline bool operator==(const Pos& other) const {
        return (x == other.x) && (y == other.y);
    }
    inline bool operator!=(const Pos& other) const {
        return (x != other.x) || (y != other.y);
    }
    inline float distance(const Pos& other){
        return hypot(y-other.y,x-other.x);
    }
};
}
// 对Pos特化std::hash
namespace std {
    template <>
    struct hash<_scene_h::Pos> {
        size_t operator()(const _scene_h::Pos& pos) const {
            return std::hash<int>()(pos.x) ^ (std::hash<int>()(pos.y) << 1);
        }
    };
}

namespace _scene_h{
// Astar节点
struct Node {
    Pos pos;
    int g; // 当前步数减去已收集的奖励数量
    int h; // 曼哈顿距离
    Node *parent;
    Node(Pos pos,int g,int h,Node *parent=nullptr):
        pos(pos),g(g),h(h),parent(parent){}
};
const vector<Pos> DIRECTIONS = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
const char *DIREC_SYMBOL="^v<>";

void display_maze(vector<vector<Cell>> maze){
    for (const auto &x : maze) {
        for (const auto &cell : x) {
            cout << static_cast<char>(cell) << " ";
        }
        cout << '\n'; // 不刷新缓冲区
    }
    cout << endl;
}
// 迷宫类
class Maze {
public:
    Maze(){}
    Maze(vector<vector<Cell>> maze,int reward=1):maze(maze),reward(reward){
        if (maze.empty() || maze[0].empty())
            throw invalid_argument("Maze cannot be empty.");
        height=maze.size();
        width=maze[0].size();
        Pos *entry_=nullptr,*end_=nullptr;
        for(int i=0;i<height;i++){
            for(int j=0;j<width;j++){
                if(maze[i][j]==Cell::start){
                    entry_=new Pos(i,j);
                }else if(maze[i][j]==Cell::end){
                    end_=new Pos(i,j);
                }
                if(entry_!=nullptr && end_!=nullptr){
                    entry=*entry_;end=*end_;
                    delete entry_;delete end_;
                    return;
                }
            }
        }
        throw invalid_argument("Maze doesn't have both the entry and end points.");
    }

    void display() {
        display_maze(maze);
    }
    void displayPath(vector<Pos> path){ // 在迷宫上显示路径
        if(path.size()<2)return;
        vector<vector<Cell>> new_maze=maze;
        for(int i=0;i<(int)path.size()-1;i++){
            if(isSolid(new_maze[path[i].y][path[i].x]))
                throw invalid_argument("Path crosses walls");
            Pos delta(path[i+1].y-path[i].y,path[i+1].x-path[i].x);
            bool found=false;
            for(int j=0;j<(int)DIRECTIONS.size();j++){
                if(delta==DIRECTIONS[j]){
                    new_maze[path[i].y][path[i].x]=(Cell)DIREC_SYMBOL[j];
                    found=true;break;
                }
            }
            if(!found) throw invalid_argument("Invalid path");
        }
        display_maze(new_maze);
    }
    vector<Pos> BestPath(Pos start,Pos dest,bool reward=false) const;
    bool isBlocked(Pos start, Pos end) const;
    vector<vector<Cell>> maze; // 迷宫的二维数组
    int width,height,reward; //reward: 方格R的奖励值
    Pos entry,end;
};

bool Maze::isBlocked(Pos start, Pos end) const{  
    // 应用 Bresenham 算法检测路径是否被阻挡  
    int x0 = start.x, y0 = start.y;  
    int x1 = end.x, y1 = end.y;  
    int dx = abs(x1 - x0), dy = abs(y1 - y0);  
    int sx = (x0 < x1) ? 1 : -1, sy = (y0 < y1) ? 1 : -1;  
    int err = dx - dy;  

    while (true) {
        //cout<<y0<<' '<<x0<<endl;
        if(y0 < 0 || x0 < 0)return true;
        if(y0 >= height || x0 >= width)return true;
        // 检查当前点是否是障碍物  
        if (isSolid(maze[y0][x0])) {
            return true; // 路径被障碍物阻挡  
        }  
        // 如果到达终点，返回 false，表示路径未被阻挡  
        if (x0 == x1 && y0 == y1) {  
            break;  
        }  

        // 计算误差并决定下一个点  
        if (err * 2 > -dy) {  
            err -= dy;  
            x0 += sx;  
        }  
        if (err * 2 < dx) {  
            err += dx;  
            y0 += sy;  
        }  
    }  

    return false; // 路径未被障碍物阻挡  
}
vector<Pos> Maze::BestPath(Pos start,Pos dest,bool reward) const{
    // 比较函数，用于优先队列
    auto cmp = [](const Node* a, const Node* b) {
        return (a->g + a->h) > (b->g + b->h);
    };

    // 开放列表（优先队列）和闭合集
    priority_queue<Node*, vector<Node*>, decltype(cmp)> openList(cmp);
    set<Pos> rewardsGot;
    vector<pair<int,vector<Pos>>> solutions;

    // 保存所有节点，便于释放内存
    unordered_map<Pos, Node*> allNodes;

    // 初始化起点
    int startH = abs(dest.y - start.y) + abs(dest.x - start.x);
    Node* startNode = new Node(start, 0, startH);
    openList.push(startNode);
    allNodes[start] = startNode;

    while (!openList.empty()) {
        Node* current = openList.top();
        openList.pop();

        if (current->pos == dest) {
            // 构建路径
            list<Pos> path;
            //cout<<"Current g:"<<current->g<<endl;
            Node *p=current;
            while (p != nullptr) {
                path.push_front(p->pos);
                p = p->parent;
            }
            solutions.push_back(make_pair(current->g+current->h,
                vector<Pos>(path.begin(),path.end())));
        }

        // 遍历邻居节点
        for (const auto& dir : DIRECTIONS) {
            int newY = current->pos.y + dir.y;
            int newX = current->pos.x + dir.x;
            Pos neighbor(newY,newX);

            // 检查边界和障碍物
            if (newY < 0 || newY >= height || newX < 0 || newX >= width || isSolid(maze[newY][newX])) continue;

            int g_delta;
            if(reward && maze[newY][newX]==Cell::end && !rewardsGot.count(neighbor)){
                rewardsGot.insert(neighbor);
                g_delta=1-reward;
            }else{
                g_delta=1; // 增加了一步的成本
            }
            int tentativeG = current->g + g_delta;
            int h = abs(dest.y - newY) + abs(dest.x - newX);

            // 如果邻居节点未被发现，或者找到更优的路径
            if (allNodes.find(neighbor) == allNodes.end() || tentativeG < allNodes[neighbor]->g) {
                Node* neighborNode = new Node(neighbor, tentativeG, h, current);
                openList.push(neighborNode);
                allNodes[neighbor]=neighborNode;
            }
        }
    }

    for (auto& pair : allNodes) delete pair.second;
    if(!solutions.size()){
        return vector<Pos>();
    }
    int f_min=INT_MAX;
    vector<Pos> bestPath;
    for(const auto &solution:solutions){
        if(solution.first<f_min){
            bestPath=solution.second;
            f_min=solution.first;
        }
    }
    return bestPath;
}
vector<vector<Cell>> readMaze(std::istream &cin_=std::cin) {
    vector<vector<Cell>> maze;
    string line;

    while (getline(cin_, line)) {
        vector<Cell> row;
        for (char ch : line) {
            if (ch != ' ') { // 忽略空格
                row.push_back((Cell)ch);
            }
        }
        if (!row.empty()) {
            maze.push_back(row);
        }
    }

    return maze;
}
}
using _scene_h::Cell;
using _scene_h::isWall;
using _scene_h::isSolid;
using _scene_h::Pos;
using _scene_h::Node;
using _scene_h::DIRECTIONS;
using _scene_h::display_maze;
using _scene_h::Maze;
using _scene_h::readMaze;