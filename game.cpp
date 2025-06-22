#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <fstream>
#include <stdexcept>
#include <vector>
#include <string>
#include <utility>
#include "stb_image.h"
#include <GL/gl.h>
#include <GL/glu.h>
#include "GL/freeglut.h"
#include "game.h"
#include "animation_obj.h"
#include "config.h"
#ifdef _WIN32
#include <windows.h>
#else
#include "dr_wav.h"
#include <alsa/asoundlib.h>
#include <thread>
#include <mutex>
#endif
using namespace std;

struct Button{
    GLuint texture;
    float left,right,bottom,top;
    bool pressedDown;
    void (*callback)();
};

const Pos DIREC_UP={-1,0},DIREC_DOWN={1,0};
const Pos DIREC_LEFT={0,-1},DIREC_RIGHT={0,1};
const Pos DIRECS_SQUARE[]={{0,0},{0,1},{1,1},{1,0}}; // 顺时针
constexpr size_t SOUND_BUFFER_SIZE = 1 << 23;
namespace Colors{
    GLfloat none[] = {0, 0, 0, 0};
    GLfloat default_color[] = {0.6, 0.3, 0.3, 1};
    GLfloat red[] = {1, 0.3, 0.3, 1};
    GLfloat bright_red[] = {1, 0.5, 0.5, 1};
    GLfloat default_emission[] = {0.6, 0.6, 0.6, 1};
    GLfloat shininess[] = {50};
    GLfloat floor_color[] = {0.6, 0.3, 0.3, 1};
    GLfloat floor_emission[] = {0.2, 0.1, 0.1, 1};
}
namespace Controller{ // 控制器
    int mouse_lastx=0,mouse_lasty=0; // 鼠标的上次坐标
    Game *game=nullptr;
    Button btns[Conf::MAX_BUTTONS];int button_idx=0;
    bool isInMenu=false;
    vector<string> gameMaps;int gameMapIndex=-1;
    int score=0; // 保存游戏结束之后的分数
    char *menuText=nullptr;
#ifndef _WIN32
    mutex playsound_lock;
#endif
}

double fmodn(double a,double b){
    // 支持负数的fmod
    double result=fmod(a,b);
    if(a<0)result=b-fabs(result);
    return result;
}
void throw_stbi_err(){
    const char *reason=stbi_failure_reason();
    size_t msglen = snprintf(nullptr, 0, "Failed to load texture: %s\n", reason);
    unique_ptr<char[]> msg(new char[msglen + 1]);
    snprintf(msg.get(),msglen,"Failed to load texture: %s\n", reason);
    throw runtime_error(msg.get());
}

// 视图部分
Pos3D convert_pos(float cam_x, float cam_y, float cam_z, float angle_xz,
                  float angle_y) {
    // 将相机角度转换为单位向量
    //if (M_PI / 2 < angle_y && angle_y < M_PI * 1.5) *flag = -1; // 相机朝下
    //else *flag = 1; // 相机朝上
    float vecLength=1;
    Pos3D target;
    target.x = cos(angle_xz) * vecLength * cos(angle_y) + cam_x;
    target.z = sin(angle_xz) * vecLength * cos(angle_y) + cam_z;
    target.y = sin(angle_y) * vecLength + cam_y;
    return target;
}
inline void drawText(const char *text,void *font=GLUT_BITMAP_HELVETICA_18){
    const char *curText = text;
    while (*curText)
        glutBitmapCharacter(font, *(curText++));
}
void displayState(){
    // 绘制文本
    glMatrixMode(GL_PROJECTION); // 切换到投影矩阵
    glPushMatrix(); // 保存当前投影矩阵
    glLoadIdentity(); // 重置投影矩阵
    gluOrtho2D(0, View::width, 0, View::height); // 设置正交投影

    glMatrixMode(GL_MODELVIEW); // 切换回模型视图矩阵
    glLoadIdentity(); // 重置模型视图矩阵

    glColor3f(1.0f, 1.0f, 1.0f);
    // 设置文本位置
    glRasterPos2f(View::width - 300, View::height - 30);
    char text[50];
    snprintf(text, sizeof(text),
             "Pos: (%.1f, %.1f, %.1f) Health: %.1f",
             View::cam_x, View::cam_y, View::cam_z,
             Controller::game->player.health);
    drawText(text);

    glRasterPos2f(View::width - 300, View::height - 55);
    memset(text,0,sizeof(text));
    snprintf(text, sizeof(text),
             "Score: %d Arrows: %d", Controller::game->score,
             Controller::game->player.rocket_cnt);
    drawText(text);

    if(Controller::game->player.invulnerableTicks>0){
        glColor3f(1.0f, 0.0f, 0.0f);
        glRasterPos2f(View::width - 125, View::height - 55); // 重设颜色后需要重设坐标，否则颜色不会改变
        memset(text,0,sizeof(text));
        drawText("Attack!");
    }
    glMatrixMode(GL_PROJECTION); // 切换回投影矩阵
    glPopMatrix(); // 恢复之前的投影矩阵
    glMatrixMode(GL_MODELVIEW); // 切换回模型视图矩阵
}
void initLighting() {
    // 启用光照
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0); // 启用光源0

    // 定义光源位置
    GLfloat light_position[] = {(float)View::maze.height/2,Conf::wall_height*10,
                                -(float)View::maze.width/2, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, light_position);

    // 定义光源颜色
    GLfloat ambient_light[] = {0.0f, 0.0f, 0.0f, 1.0f}; // 环境光
    GLfloat diffuse_light[] = {1.0f, 1.0f, 1.0f, 1.0f}; // 漫反射光
    GLfloat specular_light[] = {1.0f, 1.0f, 1.0f, 0.4f}; // 镜面光
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient_light);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse_light);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular_light);

    GLfloat constant_attenuation = 1.0f; // 常数衰减
    GLfloat linear_attenuation = 0.01f;    // 线性衰减
    GLfloat quadratic_attenuation = 0.0001f; // 二次衰减
    glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, constant_attenuation);
    glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, linear_attenuation);
    glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, quadratic_attenuation);
}
GLuint loadTexture(const char *filename, int nchannel=3, int filter=GL_LINEAR) {
    GLuint texture;
    int width, height, channels;
    unsigned char *data = stbi_load(filename, &width, &height, &channels, nchannel);
    if (data) {
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
        int mode=(nchannel==4 && channels==4)?GL_RGBA:GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, mode, width, height, 0, mode, GL_UNSIGNED_BYTE, data);
        //glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(data);
    } else {
        throw_stbi_err();
    }
    return texture;
}

void drawCeil(const Maze &maze){
    for(int y=0;y<maze.height;y++){
        for(int x=0;x<maze.width;x++){
            for(int i=0;i<4;i++){
                float dx = -(DIRECS_SQUARE[(i+1)%4].y-DIRECS_SQUARE[i].y);
                float dy = DIRECS_SQUARE[(i+1)%4].x-DIRECS_SQUARE[i].x; // dy为x的差（顺时针旋转了90°）
                float length = dx*dx+dy*dy+1;
                float unitX = dy / length, unitY = 1 / length, unitZ = dx / length;
                glNormal3f(unitX, unitY, unitZ);
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, View::ceil2Texture);
                glBegin(GL_TRIANGLES);
                glTexCoord2f(0, 1);
                glVertex3f(y+DIRECS_SQUARE[i].y, Conf::ceil_height, -(x+DIRECS_SQUARE[i].x));
                glTexCoord2f(0.5, 0);
                glVertex3f(y+0.5, Conf::ceil_height+1, -(x+0.5));
                glTexCoord2f(1, 1);
                glVertex3f(y+DIRECS_SQUARE[(i+1)%4].y, Conf::ceil_height, -(x+DIRECS_SQUARE[(i+1)%4].x));
                glEnd();
                glDisable(GL_TEXTURE_2D);
            }
        }
    }
}
void drawPillar(int y, int x){
    float height=Conf::ceil_height+1;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, View::pillarTexture);
    glNormal3f(0.0f, 0.0f, 1.0f);
    glBegin(GL_POLYGON);
    glTexCoord2f(0, 0);
    glVertex3f(y+1, height, -(x+1));
    glTexCoord2f(1, 0);
    glVertex3f(y, height, -(x+1));
    glTexCoord2f(1, 1);
    glVertex3f(y, 0, -(x+1));
    glTexCoord2f(0, 1);
    glVertex3f(y+1, 0, -(x+1));
    glEnd();

    glNormal3f(0.0f, 0.0f, -1.0f);
    glBegin(GL_POLYGON);
    glTexCoord2f(0, 0);
    glVertex3f(y+1, height, -x);
    glTexCoord2f(1, 0);
    glVertex3f(y, height, -x);
    glTexCoord2f(1, 1);
    glVertex3f(y, 0, -x);
    glTexCoord2f(0, 1);
    glVertex3f(y+1, 0, -x);
    glEnd();

    glNormal3f(1.0f, 0.0f, 0.0f);
    glBegin(GL_POLYGON);
    glTexCoord2f(0, 0);
    glVertex3f(y+1, height, -(x+1));
    glTexCoord2f(1, 0);
    glVertex3f(y+1, height, -x);
    glTexCoord2f(1, 1);
    glVertex3f(y+1, 0, -x);
    glTexCoord2f(0, 1);
    glVertex3f(y+1, 0, -(x+1));
    glEnd();

    glNormal3f(-1.0f, 0.0f, 0.0f);
    glBegin(GL_POLYGON);
    glTexCoord2f(0, 0);
    glVertex3f(y, height, -(x+1));
    glTexCoord2f(1, 0);
    glVertex3f(y, height, -x);
    glTexCoord2f(1, 1);
    glVertex3f(y, 0, -x);
    glTexCoord2f(0, 1);
    glVertex3f(y, 0, -(x+1));
    glEnd();
    glDisable(GL_TEXTURE_2D);
}
void drawLine(float y1,float x1,float y2,float x2,Pos normalVec,GLuint texture=0){
    // normalVec: 期望的法线方向，用来指示正面
    // 化为单位向量
    float dx = x2 - x1,dy = y2 - y1;
    float length = hypot(dx, dy);
    float unitX = dx / length,unitY = dy / length;
    if(normalVec.x*(-unitY)+normalVec.y*unitX<0){ // 转换为期望的法线方向
        unitX=-unitX;unitY=-unitY;
        swap(y1,y2);swap(x1,x2); //保证从法线方向看，(y1,x1)一定在左边
    }
    // 顺时针旋转90度（(x,y)变为(-y,x)），再将z轴取反 (由于OpenGL使用左手坐标系)
    glNormal3f(-unitY, 0.0f, -unitX);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBegin(GL_QUADS);
    // 约定迷宫的x绘制在z轴，y绘制在x轴
    glTexCoord2f(0, -Conf::wall_height);
    glVertex3f(y1, Conf::wall_height, -x1);
    glTexCoord2f(0, 0);
    glVertex3f(y1, 0, -x1);
    glTexCoord2f(length, 0);
    glVertex3f(y2, 0, -x2);
    glTexCoord2f(length, -Conf::wall_height);
    glVertex3f(y2, Conf::wall_height, -x2);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}
void drawFloor(int y,int x,GLuint texture=0){
    glNormal3f(0, 1, 0);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBegin(GL_QUADS);
    glTexCoord2f(0,0);
    glVertex3f(y,Conf::FLOOR_HEIGHT,-x);
    glTexCoord2f(0,1);
    glVertex3f(y,Conf::FLOOR_HEIGHT,-(x+1));
    glTexCoord2f(1,1);
    glVertex3f(y+1,Conf::FLOOR_HEIGHT,-(x+1));
    glTexCoord2f(1,0);
    glVertex3f(y+1,Conf::FLOOR_HEIGHT,-x);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}
GLuint getWallTexture(Cell block){
    if(block==Cell::wall) return View::wallTexture;
    if(block==Cell::wall2) return View::wall2Texture;
    return 0;
}
void drawMaze(const Maze &maze){
    glMaterialfv(GL_FRONT, GL_SHININESS, Colors::shininess);

    // 基础地板
    glMaterialfv(GL_FRONT, GL_EMISSION, Colors::floor_emission);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, Colors::floor_color);
    glMaterialfv(GL_FRONT, GL_SPECULAR, Colors::default_color);
    glColor3f(0.6, 0.3, 0.3);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glBegin(GL_POLYGON);
    glVertex3f(maze.height, 0, -maze.width);
    glVertex3f(0, 0, -maze.width);
    glVertex3f(0, 0, 0);
    glVertex3f(maze.height, 0, 0);
    glEnd();
    // 恢复默认材质
    glMaterialfv(GL_FRONT, GL_EMISSION, Colors::default_emission);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, Colors::default_color);
    glMaterialfv(GL_FRONT, GL_SPECULAR, Colors::default_color);

    drawCeil(maze); // 天花板

    glColor3f(1,1,1);
    for(int i=0;i<maze.height;i++){
        for(int j=0;j<maze.width;j++){
            GLuint texture;
            switch(maze.maze[i][j]){
                case Cell::start:
                    texture=View::startTexture;break;
                case Cell::end:
                    texture=View::endTexture;break;
                case Cell::auto_wall:
                    texture=View::autoWallTexture;break;
                case Cell::regen:
                    texture=View::regenTexture;break;
                case Cell::trap:
                    texture=View::trapTexture;break;
                case Cell::teleport:
                    texture=View::tpTexture;break;
                case Cell::pillar:
                    drawPillar(i,j);
                    texture=0;break;
                case Cell::rocket:
                    texture=View::rocketItemTexture;break;
                case Cell::reward:
                    texture=View::rewardTexture;break;
                case Cell::reward2:
                    texture=View::reward2Texture;break;
                case Cell::reward3:
                    texture=View::reward3Texture;break;
                case Cell::reward4:
                    texture=View::reward4Texture;break;
                default:
                    texture=0;
            }
            if(texture) // 无纹理时，使用默认的蓝色地板
                drawFloor(i,j,texture);
        }
    }
    glColor3f(1,1,1);
    // 横向扫描，绘制竖直的墙的边界
    for(int i=0;i<maze.height;i++){
        for(int j=0;j<maze.width-1;j++){
            if(isWall(maze.maze[i][j]) && !isWall(maze.maze[i][j+1])){
                drawLine(i,j+1,i+1,j+1,DIREC_RIGHT,getWallTexture(maze.maze[i][j]));
            } else if (!isWall(maze.maze[i][j]) && isWall(maze.maze[i][j+1])){
                drawLine(i,j+1,i+1,j+1,DIREC_LEFT,getWallTexture(maze.maze[i][j+1]));
            }
        }
    }
    // 纵向扫描，绘制水平的墙的边界
    for(int j=0;j<maze.width;j++){
        for(int i=0;i<maze.height-1;i++){
            if(isWall(maze.maze[i][j]) && !isWall(maze.maze[i+1][j])){
                drawLine(i+1,j,i+1,j+1,DIREC_DOWN,getWallTexture(maze.maze[i][j]));
            } else if (!isWall(maze.maze[i][j]) && isWall(maze.maze[i+1][j])){
                drawLine(i+1,j,i+1,j+1,DIREC_UP,getWallTexture(maze.maze[i+1][j]));
            }
        }
    }
}
void render_entity(const fPos &pos, float bottom, float top, float width, GLuint texture=0){
    // 渲染人物实体，pos为二维坐标
    fPos orient,delta(View::cam_x-0.5-pos.y,View::cam_z-0.5-pos.x); // 朝向观察者的向量
    float distance=hypot(delta.y,delta.x);
    if(distance!=0)
        orient=fPos(delta.y/distance*width/2,
                    delta.x/distance*width/2); // 归一化并考虑渲染宽度
    else orient=fPos(0.5,0); // 默认朝向
    fPos left(pos.y+orient.x+0.5,pos.x-orient.y+0.5); // 实际渲染时加上0.5格
    fPos right(pos.y-orient.x+0.5,pos.x+orient.y+0.5);
    
    glNormal3f(orient.y, 0.0f, -orient.x);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBegin(GL_QUADS);
    // 约定迷宫的x绘制在z轴，y绘制在x轴
    glTexCoord2f(0, 1);
    glVertex3f(left.y, bottom, -left.x);
    glTexCoord2f(0, 0);
    glVertex3f(left.y, top, -left.x);
    glTexCoord2f(1, 0);
    glVertex3f(right.y, top, -right.x);
    glTexCoord2f(1, 1);
    glVertex3f(right.y, bottom, -right.x);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}
void Player::render(){
    if(abs(View::cam_x-0.5-pos.y)>0.5 || abs(View::cam_z-0.5-pos.x)>0.5){
        View::cam_x=pos.y+0.5; // 使镜头跟随玩家的传送
        View::cam_z=pos.x+0.5;
    }
}
void Enemy::render(){
    int maxNeededTicks=ceil(pos.distance(next_pos)/speed/Conf::tick_time);
    fPos cur_pos(((float)pos.y*neededTicks+(float)next_pos.y*(maxNeededTicks-neededTicks))/maxNeededTicks,
                 ((float)pos.x*neededTicks+(float)next_pos.x*(maxNeededTicks-neededTicks))/maxNeededTicks);
    if(invulnerableTicks>0){
        glMaterialfv(GL_FRONT, GL_DIFFUSE, Colors::bright_red);
        glMaterialfv(GL_FRONT, GL_SPECULAR, Colors::bright_red);
        glMaterialfv(GL_FRONT, GL_EMISSION, Colors::red);
        glColor3f(1,0.5,0.5); // 偏红
    } else {
        glColor3f(1,1,1);
    }
    render_entity(cur_pos, 0, render_height, render_width, texture);
    // 恢复默认材质
    glMaterialfv(GL_FRONT, GL_EMISSION, Colors::default_emission);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, Colors::default_color);
    glMaterialfv(GL_FRONT, GL_SPECULAR, Colors::default_color);
}
void MiniRocket::render(){
    glColor3f(1,1,1);
    render_entity(fPos(pos.x,pos.y),pos.z,pos.z+Conf::rocket_height,
                  Conf::rocket_width, View::rocketTexture);
}
void Player::throwRocket(float angle_xz, float angle_y){
    if(rocket_cnt <= 0) return; //玩家尚未拥有火箭
    rocket_cnt -= 1;
    Pos3D target = convert_pos(0,0,0,angle_xz,angle_y);
    MiniRocket *rocket = new MiniRocket(
        *Controller::game,
        Pos3D(View::cam_z-0.5,View::cam_x-0.5,View::cam_y),
        Pos3D(-target.z,target.x,target.y)*Conf::rocket_speed,
        *this);
    game.other_entities.push_back(rocket);
}

void drawMenu();
void display() { // 渲染整个画面
    if(Controller::isInMenu){
        glDisable(GL_LIGHTING);
        drawMenu();return;
    }
    glEnable(GL_DEPTH_TEST); // 开启深度(z排序)
    glClear(GL_COLOR_BUFFER_BIT);
    glClear(GL_DEPTH_BUFFER_BIT); // 清除深度(z排序)缓冲区
    glLoadIdentity();

    Pos3D target = convert_pos(View::cam_x,View::cam_y,-View::cam_z,View::angle_xz,View::angle_y);
    // z轴取反，转换为右手坐标
    gluLookAt(View::cam_x,View::cam_y,-View::cam_z, target.x,target.y,target.z, 0, 1, 0);

    if(Conf::enable_lighting)glEnable(GL_LIGHTING);
    Controller::game->render();
    if(Conf::enable_lighting)glDisable(GL_LIGHTING);
    displayState();

    glFlush();
    glutSwapBuffers();
}
void Game::render() { // 仅渲染游戏场景和实体
    drawMaze(maze);
    player.render();
    for (Enemy *enemy : enemies) {
        enemy->render();
    }
    for (Entity *entity : other_entities) {
        entity->render();
    }
}

void handleGravity();
void gameLoop(int _){ // 主游戏循环
    if(fmod(View::game_time,Conf::tick_time) \
       > fmod(View::game_time+Conf::frame_time,Conf::tick_time))
        Controller::game->tick();
    if(!View::god_mode)handleGravity();
    display();
    View::game_time+=Conf::frame_time;
    if(!Controller::game->isover && !Controller::game->iswin){
        glutTimerFunc(Conf::frame_time*1000, gameLoop, 0);
    }
}
bool Player::isonGround(){
    return View::cam_y<=Conf::player_height;
}
void handleGravity(){
    float dt=Conf::frame_time;
    View::cam_yspeed-=Conf::g*dt;
    View::cam_y+=View::cam_yspeed*dt;
    if(View::cam_y<=Conf::player_height){
        View::cam_y=Conf::player_height;
        View::cam_yspeed=0;
    }
}

// 控制器部分
void set3dMatrix(){
    int width=View::width,height=View::height;
    // 设置当前矩阵为投影矩阵
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // 透视投影, 前4个参数效果类似FOV(视角大小),
    // 后2个参数分别是物体与相机的最近、最远距离
    float ZNEAR=Conf::ZNEAR;
    glFrustum(-ZNEAR*width/(float)height*Conf::fov_rate,
              ZNEAR*width/(float)height*Conf::fov_rate,
              -ZNEAR*Conf::fov_rate, ZNEAR*Conf::fov_rate, ZNEAR, 50);

    // 设置当前矩阵为模型视图矩阵
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glViewport(0, 0, width, height);
}
void set2dMatrix();
void resize(int width,int height){
    View::width=width;View::height=height;
    if(Controller::isInMenu)
        set2dMatrix();
    else set3dMatrix();
}
void moveCamera(float step){
    // step为正时向前移动，为负时向后
    float y_delta=cos(View::angle_xz)*step;
    float x_delta=-sin(View::angle_xz)*step; // 转换为opengl的左手坐标系
    // 二维的x对应z轴，y对应x轴
    int new_y=floor(View::cam_x+y_delta+copysign(Conf::player_width/2,y_delta));
    int new_x=floor(View::cam_z+x_delta+copysign(Conf::player_width/2,x_delta));
    bool suffocate=!Controller::game->isValidPos(Controller::game->player.pos,true); // 玩家是否窒息 (卡在墙内)
    if(suffocate || View::cam_y>=Conf::height+Conf::player_height || // 玩家在高空
        Controller::game->isValidPos(Pos(new_y,Controller::game->player.pos.x),true)){
        View::cam_x+=y_delta; // 更新y轴
        Controller::game->player.pos.y=round(View::cam_x-0.5);
        Controller::game->player.pos.y=max(0,min(
            Controller::game->player.pos.y,Controller::game->maze.height-1));
    }
    if(suffocate || View::cam_y>=Conf::height+Conf::player_height ||
        Controller::game->isValidPos(Pos(Controller::game->player.pos.y,new_x),true)){
        View::cam_z+=x_delta; // 更新x轴
        Controller::game->player.pos.x=round(View::cam_z-0.5);
        Controller::game->player.pos.x=max(0,min(
            Controller::game->player.pos.x,Controller::game->maze.width-1));
    }
}
void on_special_key(int key, int x, int y) {
    if(Controller::isInMenu)return;
    switch (key) {
        case GLUT_KEY_UP: // 上
            if(View::angle_y + M_PI / 36 < Conf::max_angle_y)
                View::angle_y += M_PI / 36; // 10°
            break;
        case GLUT_KEY_DOWN: // 下
            if(View::angle_y - M_PI / 36 > Conf::min_angle_y)
                View::angle_y -= M_PI / 36;
            break;
        case GLUT_KEY_LEFT: // 左
            View::angle_xz -= M_PI / 36;
            break;
        case GLUT_KEY_RIGHT: // 右
            View::angle_xz += M_PI / 36;
            break;
        case GLUT_KEY_SHIFT_L:
        case GLUT_KEY_SHIFT_R: // god_mode中Shift用于下降
            if(!View::god_mode)break;
            float new_y=View::cam_y-Conf::player_speed/Conf::key_repeat_rate*5; // TODO: Shift键的持续按下
            if(View::cam_y>Conf::height+Conf::player_height ||
               Controller::game->isValidPos(Controller::game->player.pos,true)){
                View::cam_y=new_y;
            } else { // 玩家在非法位置并且向下移动
                View::cam_y=Conf::height+Conf::player_height;
            }
            break;
    }
    if(Controller::game->isover || Controller::game->iswin)
        glutPostRedisplay(); // 游戏未结束时，仅在gameLoop的循环中渲染
}
void on_key(unsigned char key, int x, int y) {
    if(Controller::isInMenu)return;
    key = tolower(key);
    switch (key) {
        case 'w':
            moveCamera(Conf::player_speed/Conf::key_repeat_rate);break;
        case 's':
            moveCamera(-Conf::player_speed/Conf::key_repeat_rate);break;
        case 'a':
            on_special_key(GLUT_KEY_LEFT,x,y);break; // 转换为方向键
        case 'd':
            on_special_key(GLUT_KEY_RIGHT,x,y);break;
        case 'e':
            Controller::game->player.doAttack();break;
        case 'q':
            Controller::game->player.throwRocket(View::angle_xz,View::angle_y);
            break;
        case ' ':
            if(View::god_mode)
                View::cam_y+=Conf::player_speed/Conf::key_repeat_rate;
            else if(View::cam_y<=Conf::player_height)
                View::cam_yspeed=Conf::player_jumpspeed;
            break;
    }
    if(Controller::game->isover || Controller::game->iswin)
        glutPostRedisplay();//display();
}
void menu_onClick(int button, int state, int mouse_x, int mouse_y);
void mouse_button(int button, int state, int x, int y) {
    if(Controller::isInMenu){
        menu_onClick(button,state,x,y);
        return;
    }
    if (state == GLUT_DOWN) { // 鼠标按下
        Controller::mouse_lastx=x;
        Controller::mouse_lasty=y;
    } else if (button == GLUT_RIGHT_BUTTON && state == GLUT_UP) {
        Controller::game->player.throwRocket(View::angle_xz,View::angle_y);
    }
}
void mouse_drag(int x, int y) {
    if(Controller::isInMenu)return;
    int dx = x - Controller::mouse_lastx;
    int dy = y - Controller::mouse_lasty;
    View::angle_xz += dx / 100.0 * Conf::mouse_sensitivity;
    View::angle_y -= dy / 100.0 * Conf::mouse_sensitivity;
    //View::angle_y = fmodn(View::angle_y, M_PI * 2);
    View::angle_y = max(Conf::min_angle_y,min(Conf::max_angle_y,View::angle_y));
    Controller::mouse_lastx=x;
    Controller::mouse_lasty=y;
    if(Controller::game->isover || Controller::game->iswin)
        glutPostRedisplay();
}
void initView(){
    using namespace View;
    cam_y=Conf::player_height;
    cam_x=Controller::game->player.pos.y+0.5;
    cam_z=Controller::game->player.pos.x+0.5;
    angle_xz=angle_y=0;
}
void initFog() {
    glEnable(GL_FOG);
    GLfloat fogColor[] = {0.0f, 0.0f, 0.0f, 1.0f}; // 雾的颜色
    glFogfv(GL_FOG_COLOR, fogColor);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, 40.0f); // 雾开始的距离
    glFogf(GL_FOG_END, 50.0f); // 雾结束的距离
}

// 音乐部分
void PlaySoundFile(const char *filename);
void Player::onDamage(float power){
    char soundfile[50];int cnt=2; // 声音文件数量
    snprintf(soundfile,sizeof(soundfile),"res/sound/player_hurt%d.wav",rand()%cnt+1);
    PlaySoundFile(soundfile);
}
void Enemy::onDamage(float power){
    char soundfile[50];int cnt=2; // 声音文件数量
    snprintf(soundfile,sizeof(soundfile),"res/sound/zombie_hurt%d.wav",rand()%cnt+1);
    PlaySoundFile(soundfile);
}

// 菜单部分
void add_button(float left,float right,float bottom,float top,
    void (*callback)()=nullptr,const char *texture_file=nullptr,
    GLuint textureFilter=GL_NEAREST){
    using namespace Controller;
    if(button_idx+1>=Conf::MAX_BUTTONS)
        throw runtime_error("Buttons are full");
    btns[button_idx].callback=callback;
    btns[button_idx].left=left;
    btns[button_idx].right=right;
    btns[button_idx].bottom=bottom;
    btns[button_idx].top=top;
    btns[button_idx].pressedDown=false;
    if(texture_file)
        btns[button_idx].texture=loadTexture(texture_file,4,textureFilter);
    button_idx++;
}
void clear_buttons(){
    Controller::button_idx=0;
}
void setMenuText(const char *text){
    if(Controller::menuText)
        free(Controller::menuText);
    if(text!=nullptr && strlen(text))
        Controller::menuText=strdup(text);
}
void drawMenu(){
    using namespace Controller;
    glDisable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();

    if(menuText!=nullptr){
        float fontsize=11;
        float max_width=View::width/(float)View::height;
        float x=(-fontsize*strlen(menuText)/2)*max_width/((float)View::width/2);
        glColor3f(1,1,1);
        glRasterPos2f(x,0.5);
        drawText(menuText,GLUT_BITMAP_TIMES_ROMAN_24);
    }
    glColor3f(1,1,1);
    glEnable(GL_TEXTURE_2D);
    for(int i=0;i<button_idx;i++){
        glBindTexture(GL_TEXTURE_2D, btns[i].texture);
        glBegin(GL_QUADS);
        // 顺时针绘制
        glTexCoord2f(0, 1);
        glVertex2f(btns[i].left, btns[i].bottom);
        glTexCoord2f(0, 0);
        glVertex2f(btns[i].left, btns[i].top);
        glTexCoord2f(1, 0);
        glVertex2f(btns[i].right, btns[i].top);
        glTexCoord2f(1, 1);
        glVertex2f(btns[i].right, btns[i].bottom);
        glEnd();
    }
    glDisable(GL_TEXTURE_2D);
    glFlush();
    glutSwapBuffers();
}
void menu_onClick(int button, int state, int mouse_x, int mouse_y){
    using namespace Controller;
    int width=View::width,height=View::height;
    float max_height=1,max_width=width/(float)height; // 最大显示高度固定为1，宽度可变
    float x=(mouse_x-(float)width/2)*max_width/((float)width/2);
    float y=((float)height/2-mouse_y)*max_height/((float)height/2);
    for(int i=0;i<button_idx;i++){
        if(x>btns[i].left && x<btns[i].right && y>btns[i].bottom && y<btns[i].top){
            if(state==GLUT_DOWN){
                btns[i].pressedDown=true;
            } else if(state==GLUT_UP && btns[i].pressedDown){
                btns[i].pressedDown=false;
                PlaySoundFile("res/sound/menu_click.wav");
                if(btns[i].callback)
                    btns[i].callback();
            }
            break;
        }
    }
}
void set2dMatrix(){
    int width=View::width,height=View::height;
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(-width/(float)height, width/(float)height, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
}
void initGame(const char *filename);
void restartGame(){
    if(Controller::game){
        delete Controller::game;Controller::game=nullptr;
    }
    Controller::isInMenu=false;
    set3dMatrix(); // 重置为3D矩阵
    initGame(Controller::gameMaps[Controller::gameMapIndex].c_str());
}
void startGame(){
    Controller::gameMapIndex=0;
    restartGame();
}
void nextGame(){
    Controller::gameMapIndex++;
    restartGame();
}
void Game::onWin(){
    Controller::isInMenu=true;
    Controller::score=score; //仅赢时累加分数
    char text[50];
    bool completed=Controller::gameMapIndex+1>=(int)Controller::gameMaps.size();
    if(completed){
        snprintf(text,sizeof(text),"You've completed the game with score: %d",score);
    } else {
        snprintf(text,sizeof(text),"You win with score: %d",score);
    }
    setMenuText(text);
    clear_buttons();
    if(!completed) add_button(-0.5,0.5,0,0.18,nextGame,"res/buttons/next_map.png");
    add_button(-0.5,0.5,-0.28,-0.1,[](){exit(0);},"res/buttons/exit.png");
    set2dMatrix();
}
void Game::onFail(){
    Controller::isInMenu=true;
    clear_buttons();
    char text[50];
    snprintf(text,sizeof(text),"Game over with score: %d",score);
    setMenuText(text);
    add_button(-0.5,0.5,0,0.18,restartGame,"res/buttons/restart.png");
    add_button(-0.5,0.5,-0.28,-0.1,[](){exit(0);},"res/buttons/exit.png");
    set2dMatrix();
}
void displayStartMenu(){
    Controller::isInMenu=true;
    //setMenuText("Chinese ArchaeologyPuzzle");
    clear_buttons();
    add_button(-0.8,0.8,0.35,0.6,[](){},"res/title.png",GL_LINEAR);
    add_button(-0.5,0.5,0,0.18,startGame,"res/buttons/start.png");
    add_button(-0.5,0.5,-0.28,-0.1,[](){exit(0);},"res/buttons/exit.png");
}
// 主程序
void initGame(const char *filename){
    ifstream mapfile(filename);
    int reward,enem_cnt;
    mapfile>>reward>>enem_cnt; // 读入关卡其他信息
    vector<vector<Cell>> mazeData = readMaze(mapfile);

    srand(time(nullptr));
    Maze maze(mazeData);
    Controller::game=new Game(maze,enem_cnt,reward,Controller::score);
    initView();
    glutTimerFunc(Conf::frame_time*1000, gameLoop, 0);
}
void initGameTexture(){
    View::wallTexture=loadTexture("res/wall.png");
    View::wall2Texture=loadTexture("res/wall2.png");
    View::enemyTexture=loadTexture("res/zombie.png",4);
    View::bossTexture=loadTexture("res/muniuliuma.png",4);
    View::startTexture=loadTexture("res/spawn.png",4);
    View::endTexture=loadTexture("res/end.png",4);
    View::regenTexture=loadTexture("res/regen.png",4);
    View::tpTexture=loadTexture("res/teleport.png",4);
    View::trapTexture=loadTexture("res/magma.png",4,GL_NEAREST);
    View::autoWallTexture=loadTexture("res/auto_wall.png",4);
    View::ceil2Texture=loadTexture("res/ceil2.png",4);
    View::pillarTexture=loadTexture("res/pillar.png",4);
    View::rocketTexture=loadTexture("res/rocket.png",4);
    View::rocketItemTexture=loadTexture("res/rocket_item.png",4);
    View::rewardTexture=loadTexture("res/SiNan.png",4);
    View::reward2Texture=loadTexture("res/xianglu.png",4);
    View::reward3Texture=loadTexture("res/huntian_yi.png",4);
    View::reward4Texture=loadTexture("res/suanpan.png",4);
}
void initMap(const char *filename){
    ifstream file(filename);
    while(!file.eof()){
        string line;
        getline(file,line);
        Controller::gameMaps.push_back(line);
    }
}
// -- 平台相关代码 --
#ifndef _WIN32
void _playSoundThread(string filename){ // Linux的播放线程
    lock_guard<mutex> lock(Controller::playsound_lock);
    drwav wav;
    if (!drwav_init_file(&wav, filename.c_str(), nullptr))
        throw runtime_error(string("Failed to open WAV file ")+filename);

    size_t totalSamples = wav.totalPCMFrameCount * wav.channels;
    std::unique_ptr<int16_t[]> pcmData(new int16_t[totalSamples]);

    if (drwav_read_pcm_frames_s16(&wav, wav.totalPCMFrameCount, pcmData.get()) != wav.totalPCMFrameCount) {
        drwav_uninit(&wav);
        throw runtime_error("Failed to read PCM frames");
    }

    snd_pcm_t* handle = nullptr;
    if (snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        drwav_uninit(&wav);
        throw runtime_error("Failed to open ALSA device");
    }

    snd_pcm_hw_params_t* params;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(handle, params);
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(handle, params, wav.channels);
    unsigned int rate = wav.sampleRate;
    snd_pcm_hw_params_set_rate_near(handle, params, &rate, 0);
    snd_pcm_hw_params(handle, params);

    snd_pcm_writei(handle, pcmData.get(), wav.totalPCMFrameCount);

    snd_pcm_drain(handle);
    snd_pcm_close(handle);
    drwav_uninit(&wav);
}
#endif
void PlaySoundFile(const char *filename) {
#ifdef _WIN32
    PlaySound(filename, NULL, SND_FILENAME | SND_ASYNC);
#else
    thread(_playSoundThread,string(filename)).detach();
#endif
}
void initIcon(){
#ifdef _WIN32
    HDC hdc = wglGetCurrentDC();
    HWND hwnd = WindowFromDC(hdc);
    HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(1));
    SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
#endif
}
void HDPI_support(){
#ifdef WIN32
    HMODULE hModule = LoadLibraryA("shcore.dll");
    if (hModule) {
        typedef HRESULT (WINAPI *SetDpiFunc)(int);
        SetDpiFunc SetProcessDpiAwareness = (SetDpiFunc)GetProcAddress(hModule, "SetProcessDpiAwareness");
        if (SetProcessDpiAwareness) {
            SetProcessDpiAwareness(1); // 1: PROCESS_SYSTEM_DPI_AWARE
        }
        FreeLibrary(hModule);
    }
#endif
}
unique_ptr<char> codeConv(const char *utf8String){
    // 将utf-8字符串转换为平台的编码字符串，避免乱码
#ifdef WIN32
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, utf8String, -1, nullptr, 0);
    if (wideLen == 0) return nullptr;
    wchar_t* wideStr = new wchar_t[wideLen];
    MultiByteToWideChar(CP_UTF8, 0, utf8String, -1, wideStr, wideLen);

    int ansiLen = WideCharToMultiByte(CP_ACP, 0, wideStr, -1, nullptr, 0, nullptr, nullptr);
    if (ansiLen == 0) {
        delete[] wideStr;
        return nullptr;
    }
    char* ansiStr = new char[ansiLen];
    WideCharToMultiByte(CP_ACP, 0, wideStr, -1, ansiStr, ansiLen, nullptr, nullptr);
    delete[] wideStr;
    return unique_ptr<char>(ansiStr); // 返回 ANSI 编码的字符串
#else
    // 直接返回utf8string本身，不转换
    size_t len = strlen(utf8String) + 1;
    char* result = new char[len];
    strcpy(result, utf8String);
    return unique_ptr<char>(result);
#endif
}

int main(int argc, char** argv) {
    const char *filename;
    if(argc>=2)filename=argv[1];
    else filename="mapsList.txt";
    initMap(filename);

    HDPI_support();
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_MULTISAMPLE);
    glutInitWindowSize(Conf::WIDTH, Conf::HEIGHT);
#ifdef _WIN32
    glutCreateWindow(codeConv(u8"太古神机 · 迷域寻踪").get());
#else
    glutCreateWindow("Chinese ArchaeologyPuzzle"); // TODO: 修复X11下窗口标题的乱码
#endif
    initIcon();

    glClearColor(0, 0, 0, 1);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glutDisplayFunc(display);
    glutReshapeFunc(resize);
    glutKeyboardFunc(on_key);
    glutSpecialFunc(on_special_key);
    glutMotionFunc(mouse_drag);
    glutMouseFunc(mouse_button);
    initFog();
    initGameTexture();
    if(Conf::enable_lighting)initLighting();
    displayStartMenu();
    glutMainLoop();
    delete Controller::game;
    return 0;
}