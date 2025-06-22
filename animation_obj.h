#include <iostream>
#include <cmath>
#include "game.h"

namespace _aniobj_h{
using namespace std;

class MiniRocket:public Entity{
public:
    MiniRocket(Game &game, Pos3D pos, Pos3D speed, const Entity &thrower):
        Entity(game),pos(pos),speed(speed),thrower(thrower),onGround(false){}
    void tick() override;
    void render() override;
    Pos3D pos,speed;
    const Entity &thrower; // 投掷者
    bool onGround;
};
void MiniRocket::tick(){
    if(onGround) return; // 地面的箭无法造成伤害
    if(abs(pos.x - (game.player.pos.x+0.5)) < 2 && \
       abs(pos.y - (game.player.pos.y+0.5)) < 2 && \
       &thrower != &game.player){ // 确保不会伤到自身
        game.player.doDamage(Conf::rocket_damage);
        removed=true;return;
    }
    for(Enemy *enemy:game.enemies){
        if(abs(pos.x - (enemy->pos.x+0.5)) < 2 && \
           abs(pos.y - (enemy->pos.y+0.5)) < 2 && \
           &thrower != enemy){
            enemy->doDamage(Conf::rocket_damage);
            removed=true;return;
        }
    }
    speed.z -= Conf::g * Conf::tick_time;
    pos.x += speed.x * Conf::tick_time; // x和y为二维迷宫坐标
    pos.y += speed.y * Conf::tick_time;
    if(!game.isValidPos(Pos(pos.y,pos.x),true)){
        speed.x=speed.y=0; // 碰撞到墙壁
    }
    if(pos.z <= Conf::rocket_ground_height){
        speed.x=speed.y=speed.z=0; //到达地面
        pos.z = Conf::rocket_ground_height;
        onGround = true;
    } else {
        pos.z += speed.z * Conf::tick_time;
    }
}
}
using _aniobj_h::MiniRocket;

// --Boss--
void Boss::checkAndAttack(){
    if(target && canSee(*target)){
        if(attackTicks>0) return;
        float height = Conf::boss_height;
        float distance=hypot(target->pos.x-pos.x,target->pos.y-pos.y);
        float fall_time=sqrt(2*(height-Conf::rocket_ground_height)/Conf::g);
        float speed=distance/fall_time;
        MiniRocket *rocket = new MiniRocket(
            game,
            Pos3D(pos.x,pos.y,height),
            Pos3D(target->pos.x-pos.x,target->pos.y-pos.y,0)*speed,
            *this); // 火箭远程攻击
        game.other_entities.push_back(rocket);
        attackTicks=attackSpeed; // 限制攻击速度
    }
}