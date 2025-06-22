#pragma once
#include <cstdlib>
#include <cctype>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <list>
#include <optional>
#include <GL/gl.h> // GLuint
#include "scene.h"
#include "config.h"

#define DEFAULT_ASSIGN(other) {\
    if (this == &other) return *this;\
    memcpy(this,&other,sizeof(decltype(other)));\
    return *this;\
}
#define DEFAULT_ASSIGN_FUNC(Typename) \
    inline Typename& operator=(const Typename& other){\
        DEFAULT_ASSIGN(other);\
    }
#define NOT_IMPLEMENTED {\
    throw runtime_error("virtual function not implemented");}

namespace _game_h{
using namespace std;

// 工具函数/类
struct fPos{ // 浮点数的坐标，y, x分别表示行、列
    float x;float y;
    fPos():x(0),y(0){}
    fPos(float y,float x):x(x),y(y){}
    fPos(Pos pos):x(pos.x),y(pos.y){}
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
struct Pos3D{ // 浮点数3d坐标，y, x分别表示行、列，z表示高度
    float x,y,z;
    Pos3D():x(0),y(0),z(0){}
    Pos3D(float y,float x,float z):x(x),y(y),z(z){}
    Pos3D(const Pos3D &pos):x(pos.x),y(pos.y),z(pos.z){}
    inline bool operator==(const Pos3D& other) const {
        return (x == other.x) && (y == other.y) && (z == other.z);
    }
    inline bool operator!=(const Pos3D& other) const {
        return (x != other.x) || (y != other.y) || (z != other.z);
    }
    inline float distance(const Pos3D& other) {
        return sqrt(pow(y - other.y, 2) + pow(x - other.x, 2) + pow(z - other.z, 2));
    }
    inline Pos3D operator*(float scalar) const {
        return Pos3D(y * scalar, x * scalar, z * scalar);
    }
};
template<typename T>
void delete_removed_entity(T &entities){ // 移除已移除的实体
    for(int i=0;i<(int)entities.size();i++){
        if(entities[i]->removed){
            delete entities[i];entities[i]=nullptr;
        }
    }
    entities.erase(remove_if(entities.begin(),entities.end(),
        [](const auto *ent){return !ent;}),entities.end());
}

// 模型部分
class Game;
class Entity{
public:
    Entity(Game &game);
    DEFAULT_ASSIGN_FUNC(Entity);
    virtual ~Entity() = default;
    Game &game;
    bool removed; // 将要被清除
    virtual void tick(){NOT_IMPLEMENTED;};
    virtual void render(){NOT_IMPLEMENTED;}; // 属于视图部分
};
class MobileEntity:public Entity{
public:
    MobileEntity(Game &game,Pos spawn_pos,
                float max_health,float speed,
                int invulnerability,int attackSpeed,
                float melee_power,float melee_dist);
    virtual bool doDamage(float power); // 属于模型部分
    virtual void onDamage(float power)=0; // 属于视图部分，每次doDamage之后调用，即使免疫刻数不为0
    virtual void onDeath()=0; // 属于模型或视图
    void resetAttackTicks(int speed=-1);
    void tick() override;
    float health,max_health,lastDamagePower; // lastDamagePower: 上次受到的伤害大小，用于伤害免疫
    Pos pos;float speed;
    float melee_power,melee_dist;
    int invulnerability; // 受击后伤害免疫时间
    int invulnerableTicks; // 受击后剩余伤害免疫时间
    int attackTicks; // 剩下的可攻击刻数，如果为0可立即近战攻击
    int attackSpeed; // 攻击速度 (刻)
    static bool doMeleeAttack(
        MobileEntity &attacker,MobileEntity &attackee,float power);
};
class Player:public MobileEntity{
public:
    Player(Game &game,Pos spawn_pos);
    void render() override;
    void onDamage(float power) override;
    void onDeath() override;
    void tick() override;
    void doAttack();
    void throwRocket(float angle_xy,float angle_z); // 投掷火箭
    bool isonGround(); // 需要视图部分实现
    bool can_group_attack;
    int rocket_cnt;
    optional<Pos> passedAutoWallPos; // 走过但未升起的自动墙坐标
};
class Enemy:public MobileEntity{
public:
    Enemy(Game &game,Pos pos,float speed,
          GLuint texture,float render_height,float render_width,
          float attack_speed=Conf::enemyAttackSpeed,
          float health=Conf::enemy_health);
    void render() override;
    bool doDamage(float power) override;
    void onDamage(float power) override;
    void onDeath() override;
    bool canSee(MobileEntity &other);
    inline void updateNeededTicks();
    void resetPath();
    void doFollowPath();
    void doRandomWalk();
    bool findTarget();
    virtual void checkAndAttack();
    void tick() override;
    vector<Pos> path; // 当前到目标的路径
    int path_index;
    Pos next_pos; // 目标位置 (坐标为整数)
    MobileEntity *target;
    Pos last_target_pos;
    int neededTicks;
    GLuint texture;
    float render_height,render_width;
    bool isBoss; // 是否不击败无法通关
};
class Boss:public Enemy{
public:
    Boss(Game &game, Pos pos, float speed, GLuint texture,
         float render_height, float render_width);
    void checkAndAttack() override;
};
class Game{
public:
    Game(Maze maze,
        int enem_cnt=Conf::defaultEnemCnt,
        int reward=Conf::defaultReward,
        int score=0);
    bool isValidPos(Pos pos,bool ignoreEntities=false) const;
    void tick();
    void render();
    void onGetReward();
    void onWin();void onFail();
    bool hasBossLeft();
    Maze maze;
    Player player;
    vector<Enemy *> enemies;
    vector<Entity *> other_entities; // 备用
    bool iswin,isover;
    int score,reward;
};

// --Entity的实现--
Entity::Entity(Game &game):game(game),removed(false){}
// --MobileEntity--
MobileEntity::MobileEntity(Game &game,Pos spawn_pos,
            float max_health,float speed,
            int invulnerability,int attackSpeed,
            float melee_power,float melee_dist):
    Entity(game),pos(spawn_pos),health(max_health),
    max_health(max_health),speed(speed),lastDamagePower(0),
    invulnerability(invulnerability),attackSpeed(attackSpeed),
    invulnerableTicks(0),attackTicks(0),
    melee_power(melee_power),melee_dist(melee_dist){}
void MobileEntity::tick(){
    if(invulnerableTicks>0){
        invulnerableTicks--;
    } else lastDamagePower=0;
    if(attackTicks>0)attackTicks--;
}
bool MobileEntity::doDamage(float power){
    if(invulnerableTicks>0){
        float pre_lastdamage = lastDamagePower;
        lastDamagePower = power;
        power -= pre_lastdamage; // 减少上次伤害量
    } else {
        lastDamagePower = power;
    }
    if(power<=0) return false;
    health-=power;
    invulnerableTicks=invulnerability; // 重置免疫刻数
    if(health<=0)onDeath();
    else onDamage(power);
    return true;
}
bool MobileEntity::doMeleeAttack(
    MobileEntity &attacker,MobileEntity &attackee,float power){
    if(attacker.attackTicks>0)
        return false;
    bool result = attackee.doDamage(power);
    attacker.resetAttackTicks(); // 限制攻击者的攻击速度
    return result; // 返回是否攻击成功
}
void MobileEntity::resetAttackTicks(int speed){
    if(speed==-1) speed=attackSpeed;
    attackTicks=speed; // 重置攻击冷却时间
}
// --Player--
Player::Player(Game &game,Pos spawn_pos):
    MobileEntity(game,spawn_pos,
                Conf::player_health,Conf::player_speed,
                Conf::playerInvulnerableTicks,
                Conf::playerAttackSpeed,
                Conf::player_melee_power,
                Conf::player_melee_dist),
                can_group_attack(Conf::player_group_attack),
                rocket_cnt(0){}
void Player::tick(){
    MobileEntity::tick();
    int tries=0,max_tries=10; // 传送尝试
    switch(game.maze.maze[pos.y][pos.x]){
        case Cell::end:
            if(!game.hasBossLeft()){ // 所有boss均被击败
                game.iswin=true;
                game.onWin();
            }
            break;
        case Cell::reward:
        case Cell::reward2:
        case Cell::reward3:
        case Cell::reward4:
            game.maze.maze[pos.y][pos.x]=Cell::empty;
            game.onGetReward();break;
        case Cell::trap:
            if(isonGround())
                doDamage(Conf::trap_damage);
            break;
        case Cell::teleport:
            while(tries<max_tries){
                int y=rand()*(game.maze.height/(float)RAND_MAX);
                int x=rand()*(game.maze.width/(float)RAND_MAX);
                Pos new_pos(y,x);
                if(game.isValidPos(new_pos)){
                    pos=new_pos; // 随机传送玩家
                    break;
                }
                tries++;
            }
            break;
        case Cell::regen:
            if(health<max_health){
                game.maze.maze[pos.y][pos.x]=Cell::empty;
                health=min(max_health,health+Conf::regen_health);
            }
            break;
        case Cell::rocket:
            game.maze.maze[pos.y][pos.x]=Cell::empty;
            rocket_cnt+=2;break;
        default:
            break;
    }
    // 上升柱(自动墙)
    switch(game.maze.maze[pos.y][pos.x]){
        case Cell::auto_wall:
            passedAutoWallPos = pos;
            break;
        default:
            if(passedAutoWallPos.has_value()){
                Pos wallPos = passedAutoWallPos.value();
                game.maze.maze[wallPos.y][wallPos.x]=Cell::wall;
                game.onGetReward();
                passedAutoWallPos = nullopt;
            }
    }
}
void Player::onDeath(){
    rocket_cnt=0; // 清空获得的箭
    game.isover=true;
    game.onFail();
}
void Player::doAttack(){
    for(Enemy *target:game.enemies){
        if(pos.distance(target->pos)<melee_dist){
            MobileEntity::doMeleeAttack(*this,*target,melee_power);
            if(!can_group_attack)break;
        }
    }
}
// --Enemy--
Enemy::Enemy(Game &game,Pos pos,float speed,GLuint texture,
             float render_height,float render_width,
             float attack_speed,float health):
    MobileEntity(game,pos,health,speed,
                Conf::enemyInvulnerableTicks,
                attack_speed,
                Conf::enemy_melee_power,
                Conf::enemy_melee_dist),
    neededTicks(1),next_pos(pos),
    path_index(0),target(nullptr),last_target_pos(-1,-1),
    texture(texture),render_height(render_height),
    render_width(render_width),isBoss(false){}
bool Enemy::doDamage(float power){
    bool result = MobileEntity::doDamage(power);
    if(target)resetPath();
    return result;
}
void Enemy::onDeath(){
    game.score+=game.reward*3;
    removed=true;
}
bool Enemy::canSee(MobileEntity &other){
    return !game.maze.isBlocked(pos,other.pos);
}
inline void Enemy::updateNeededTicks(){
    // 需要先设置好next_pos和pos
    float distance=pos.distance(next_pos);
    neededTicks=ceil(distance/speed/Conf::tick_time);
}
void Enemy::resetPath(){
    // 刚找到目标或目标位置改变时，重置路径，本身不重置neededTicks
    last_target_pos=target->pos;
    path=game.maze.BestPath(pos,target->pos);
    if(path.empty())target=nullptr; // 放弃追踪target
    path_index=0;
    if(path.size()>=2){
        pos=path[path_index];
        next_pos=path[path_index+1];
    } else {
        next_pos=pos;
    }
}
void Enemy::doFollowPath(){
    if(neededTicks>=0)neededTicks--; // 最终保持在-1
    if(neededTicks==0){
        path_index++;
        if(path_index>=(int)path.size()-1){ // 到达终点
            neededTicks=-1; // 不再更新
        } else {
            pos=path[path_index];
            next_pos=path[path_index+1];
            updateNeededTicks();
        }
    }
}
void Enemy::doRandomWalk(){
    if(neededTicks>=0)neededTicks--;
    if(neededTicks==0){
        pos=next_pos;
        while(true){
            Pos direc=DIRECTIONS[rand()*(DIRECTIONS.size()/(float)(RAND_MAX+1))];
            Pos newpos(pos.y+direc.y,pos.x+direc.x);
            if(game.isValidPos(newpos)){
                next_pos=newpos;
                updateNeededTicks();
                break;
            }
        }
    }
}
bool Enemy::findTarget(){
    if(canSee(game.player)){
        if(target!=&game.player)
            resetAttackTicks(attackSpeed/3);
        target=&game.player;
        return true;
    }
    return false;
}
void Enemy::checkAndAttack(){
    if(target && pos.distance(target->pos)<melee_dist){
        MobileEntity::doMeleeAttack(*this,*target,melee_power);
    }
}
void Enemy::tick(){
    MobileEntity::tick();
    switch(game.maze.maze[pos.y][pos.x]){
        case Cell::trap:
            doDamage(Conf::trap_damage);
            break;
        default:
            break;
    }
    findTarget();
    checkAndAttack();
    if(target && target->health <= 0)
        target = nullptr; // 目标死亡
    if(target){
        bool first=path.empty();
        if(first || target->pos!=last_target_pos){
            resetPath();
            if(path.size()>=2){
                if(first || neededTicks<=0)
                    updateNeededTicks();
            } else if(!path.empty()){
                neededTicks=1; // 在下一刻标记为已到达
            }
        }
        if(!path.empty())doFollowPath(); // 已找到路径
        else neededTicks=1; // 找不到路径，在下一刻开始随机游走
    } else {
        doRandomWalk();
    }
}
// --Boss--
Boss::Boss(Game &game, Pos pos, float speed, GLuint texture,
           float render_height, float render_width)
    : Enemy(game, pos, speed, texture, render_height, render_width,
            Conf::bossAttackSpeed, Conf::boss_health){isBoss = true;}
// --Game--
Game::Game(Maze maze,int enem_cnt,int reward,int score):
    maze(maze),player(*this,Pos(0,0)),iswin(false),
    isover(false),score(score),reward(reward){
    int tries=0,max_tries=maze.width*maze.height*1000;
    player=Player(*this,maze.entry); // 在入口生成玩家
    for(int i=0;i<enem_cnt;i++){
        // 随机生成敌人
        while(tries<max_tries){
            int y=rand()*(maze.height/(float)RAND_MAX);
            int x=rand()*(maze.width/(float)RAND_MAX);
            Pos enem_pos(y,x);
            if(isValidPos(enem_pos)){
                enemies.push_back(new Enemy(*this,enem_pos,Conf::enemy_speed,
                                           View::enemyTexture,
                                           Conf::enemy_height, Conf::enemy_width));
                tries=0;break;
            }
            tries++;
        }
        if(tries)throw runtime_error("No free spaces in scene available");
    }
    for(int y=0;y<maze.height;y++){
        for(int x=0;x<maze.width;x++){
            if(maze.maze[y][x] == Cell::boss){
                enemies.push_back(new Boss(*this,Pos(y,x),Conf::boss_speed,
                                           View::bossTexture,
                                           Conf::boss_height, Conf::boss_width));
            }
        }
    }
}
bool Game::isValidPos(Pos pos,bool ignoreEntities) const{
    if(pos.y<0 || pos.x<0)return false;
    if(pos.y>=maze.height || pos.x>=maze.width)return false;
    if(isSolid(maze.maze[pos.y][pos.x]))return false;
    if(ignoreEntities)return true;
    if(pos==player.pos)return false;
    for(const Enemy *enemy:enemies){
        if(pos==enemy->pos)return false;
    }
    return true;
}
void Game::tick(){
    delete_removed_entity(enemies);
    delete_removed_entity(other_entities);

    player.tick();
    for(Enemy *enemy:enemies){
        enemy->tick();
    }
    for(Entity *entity:other_entities){
        entity->tick();
    }
}
void Game::onGetReward(){
    score+=reward;
}
bool Game::hasBossLeft(){
    for(Enemy *enemy:enemies){
        if(enemy->isBoss) return true;
    }
    return false;
}
}
using _game_h::fPos;
using _game_h::Pos3D;
using _game_h::Entity;
using _game_h::MobileEntity;
using _game_h::Player;
using _game_h::Enemy;
using _game_h::Game;
using _game_h::Boss;