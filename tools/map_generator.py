# 地图随机生成工具（备用）
import sys,random
sys.setrecursionlimit(10000)

WALL='W';SPACE='_'
def generate_maze(width, height):
    # 初始化迷宫
    maze = [[WALL] * (width * 2 + 1) for _ in range(height * 2 + 1)]

    # 定义方向
    directions = [(0, 1), (1, 0), (0, -1), (-1, 0)]

    def carve(x, y):
        maze[y][x] = SPACE
        random.shuffle(directions)
        for dx, dy in directions:
            nx, ny = x + dx * 2, y + dy * 2
            if 0 < nx < width * 2 and 0 < ny < height * 2 and maze[ny][nx] == WALL:
                maze[y + dy][x + dx] = SPACE
                carve(nx, ny)

    carve(1, 1)
    return maze

def print_maze(maze):
    for row in maze:
        print(' '.join(row))

# 生成并打印迷宫
reward=10;enem_cnt=10
maze = generate_maze(20, 20)

print(reward,enem_cnt)
print_maze(maze)