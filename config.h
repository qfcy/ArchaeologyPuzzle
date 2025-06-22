#pragma once
#include <GL/gl.h> // GLuint

// -- 游戏部分 --
namespace Conf{
    const int height=3; // 场景高度，单位m
    constexpr float tick_time=0.05;
    const int FPS=60;
    constexpr float frame_time=1/(float)FPS;
    const int defaultEnemCnt=8;
    const int defaultReward=10;
    const float g=9.8; // 重力加速度
    // 玩家和敌人
    const float player_health=20;
    const float player_speed=2.5; // m/s
    const float player_melee_dist=1.5;
    const float player_melee_power=3;
    constexpr int playerInvulnerableTicks=0.5/tick_time; // 受击后免疫0.5s
    constexpr int playerAttackSpeed=0.3/tick_time; // 0.3s攻击一次
    bool player_group_attack=true;
    const float enemy_health=20;
    const float enemy_speed=1.2;
    const float enemy_melee_dist=1.2;
    const float enemy_melee_power=3;
    const float boss_health=25;
    const float boss_speed=1.2;
    constexpr int enemyInvulnerableTicks=0.5/tick_time;
    constexpr int enemyAttackSpeed=1/tick_time; // 1s攻击一次
    constexpr int bossAttackSpeed=2/tick_time; // 2s攻击一次
    const float rocket_damage = 8; // 火箭伤害
    // 机关装置
    const float trap_damage=1; // 陷阱伤害，对玩家和敌人有效
    const float regen_health=2; // 仅玩家可以用来恢复生命值
}
// -- 渲染部分 --
namespace Conf{
    const float wall_height=2.5; // 墙高度，单位m
    const float ceil_height=4; // 天花板高度
    const float player_height=1.65; // 玩家观察高度
    const float player_width=0.6; // 玩家碰撞箱宽度
    const float player_jumpspeed=5; // 跳跃速度，5m/s对应高度约1.25m
    const float enemy_height=1.2; // 敌人渲染高度
    const float enemy_width=0.7; // 敌人渲染宽度
    const float boss_height=1.2;
    const float boss_width=1.0;
    const float rocket_height = 0.5;
    const float rocket_width = 0.3;
    const float rocket_speed = 10;
    const float rocket_ground_height = 0.2; // 火箭到达地面的判定高度

    const float ZNEAR=0.02;
    const float FLOOR_HEIGHT=8e-3; // 地板渲染高度
    const int MAX_BUTTONS=100;
    const float max_angle_y=M_PI/2-1e-3;
    const float min_angle_y=-M_PI/2+1e-3;
    const int WIDTH=800,HEIGHT=600;
    const int key_repeat_rate=30;
    const float fov_rate=0.7;
    const bool enable_lighting=true; // 是否启用光照
    const float mouse_sensitivity=1; // 鼠标灵敏度
}
namespace View{ // 视图
    float angle_xz=0,angle_y=0;
    float cam_x,cam_y,cam_z; // cam_z为实际相机z坐标的相反数，由于opengl使用左手坐标系
    int width,height;
    Maze maze;
    GLuint wallTexture,wall2Texture;
    GLuint enemyTexture,bossTexture;
    GLuint startTexture,endTexture;
    GLuint trapTexture,regenTexture;
    GLuint tpTexture,autoWallTexture;
    GLuint ceil2Texture,pillarTexture;
    GLuint rocketTexture, rocketItemTexture; // rocketItemTexture用于物品形式
    GLuint rewardTexture,reward2Texture,reward3Texture,reward4Texture;
    double game_time=0;
    bool god_mode=false; // 飞行模式
    float cam_yspeed=0;
}