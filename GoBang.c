// ncurses五子棋界面程序
// Benny 2021.6 @ School of Computer & Commuinication, HNIE
// 
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <ncurses.h>
#include <signal.h>
#include  <pthread.h>
#include <semaphore.h>

#include<stdio.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<sys/types.h>
#include<netdb.h>



#define LISTENQ 1024
#define MAXLINE 1024
typedef struct sockaddr SA;

//常量定义
#define CKBD_DIMENSION_SZ				(19)		//棋盘大小
#define POSITION_BLANK_CH		 		('+')		//空白无棋子
#define PIECE_BLACK_CH					('H')		//黑棋子
#define PIECE_WHITE_CH					('O')		//白棋子
#define PIECE_CANDIDATE_CH			    ('X')		//选点棋子

//数据定义
typedef struct {
  char piece_color;		//本方棋子颜色
  char active_color;	//当前下子方颜色
  int  steps;					//当前步数
  //棋局开始时间
  //本回合开始时间
  //其它状态
}game_status;

struct place_info {
    int x;
    int y;
    int  stepes;
};

//全局变量
char g_host_ip[32];  //主机IP地址, 客方需要
short g_tcp_port;    //主机TCP通信端口
char g_ckbd_situation[CKBD_DIMENSION_SZ][CKBD_DIMENSION_SZ];	//棋局矩阵
game_status  g_status;
char g_host_piece_color = PIECE_WHITE_CH;  //主机棋子颜色
char g_guest_piece_color = PIECE_BLACK_CH;  //客机棋子颜色
int g_host_flag = 0;
sem_t mutex;//定义信号量
struct place_info g_step_info;


//---------------------------------------------------
// 显示棋局
void show_situation(int cand_x, int cand_y)
{
  char * P_CAPTION = "== My GoBang Game ==";//标题
  int x, y;	
	
	x = (CKBD_DIMENSION_SZ * 2 + 4 - strlen(P_CAPTION))/2;//计算标题打印的位置
  mvwprintw(stdscr, 0, x, "%s", P_CAPTION);  //打印标题
  for(x = 0; x < CKBD_DIMENSION_SZ; x++) {  //显示列头  	
    mvwprintw(stdscr, 1, x * 2 + 4, "%c", 'a' + x);  //列序号
  }
  for(y = 0; y < CKBD_DIMENSION_SZ; y++) {
    mvwprintw(stdscr, y + 2, 1, "%d", y + 1);  //行序号
    for(x = 0; x < CKBD_DIMENSION_SZ; x++) {
    	mvwprintw(stdscr, y + 2, x * 2 + 4, "%c", g_ckbd_situation[y][x]); //交叉点显示
    }
  }
  if(cand_x >= 0 && cand_x < CKBD_DIMENSION_SZ &&
     cand_y >= 0 && cand_y < CKBD_DIMENSION_SZ &&
    g_ckbd_situation[cand_y][cand_x] == POSITION_BLANK_CH) {//如果棋子再棋盘内，并且此处没有放棋子    
    mvwprintw(stdscr, cand_y + 2, cand_x * 2 + 4, "%c", PIECE_CANDIDATE_CH);//打印选点棋子
    mvwprintw(stdscr, CKBD_DIMENSION_SZ + 2, 4, "%c (%d, %c)", PIECE_CANDIDATE_CH, cand_y + 1, 'a' + cand_x);//打印大当前坐标
  }
  wrefresh(stdscr);//讲内存中的界面显示到桌面
}

// 向某方向获取待下棋子位置（移动待下棋子位置）
int get_candidate_piece_pos(int * p_x, int * p_y, int direction)
{
  int x = *p_x;
  int y = *p_y;
  int dir = direction;
  int dir_i = 0;
  int all_dirs[4] = {KEY_RIGHT, KEY_LEFT, KEY_DOWN, KEY_UP};//上下左右键
  
  if(*p_x == -1 || *p_y == -1) { //未确定当前位置
    *p_x = CKBD_DIMENSION_SZ/2;  //从中心点开始
    *p_y = CKBD_DIMENSION_SZ/2;
    if(g_ckbd_situation[*p_y][*p_x] == POSITION_BLANK_CH) {
      return 0; //当前位置可用
    }
  }
  if(direction == 0)
    dir = all_dirs[dir_i];
  while(dir_i <= 4) {
    if(dir == KEY_RIGHT) { //向右移  	
      for(x++; x < CKBD_DIMENSION_SZ; x++) {
        if(g_ckbd_situation[y][x] == POSITION_BLANK_CH) { //当前位置可用
		    *p_x = x; *p_y = y;
          break;
        }
      }      
    }
    else if(dir == KEY_LEFT) { //向左移
  	  for(x--; x >= 0; x--) {
        if(g_ckbd_situation[y][x] == POSITION_BLANK_CH) { //当前位置可用
		    *p_x = x; *p_y = y;
          break;
        }
      }
    }
    else if(dir == KEY_DOWN) { //向下移
  	  for(y++; y < CKBD_DIMENSION_SZ; y++) {
        if(g_ckbd_situation[y][x] == POSITION_BLANK_CH) { //当前位置可用
		    *p_x = x; *p_y = y;
          break;
        }
      }
    }
    else if(dir == KEY_UP) { //向上移
      for(y--; y >= 0; y--) {
        if(g_ckbd_situation[y][x] == POSITION_BLANK_CH) { //当前位置可下子
		 *p_x = x; *p_y = y;
          break;
        }
      }
    }
    if(direction == 0)
      dir = all_dirs[dir_i++];  //换一个方向再试
    else
      break;
  }  
  if(g_ckbd_situation[*p_y][*p_x] == POSITION_BLANK_CH) {//如果找到了没有被落子的位置
    return 0;  //原位置
  }
  return -1;
}

// 显示信息
void show_message(const char * msg)
{
  mvwprintw(stdscr, CKBD_DIMENSION_SZ + 4, 1, "MSG: %s", msg);
  
}

// 显示状态信息
void show_status()
{
  mvwprintw(stdscr, 1, CKBD_DIMENSION_SZ * 2 + 6, "-- STATUS --");
  mvwprintw(stdscr, 2, CKBD_DIMENSION_SZ * 2 + 6, "Color : %c", g_status.piece_color);
  mvwprintw(stdscr, 3, CKBD_DIMENSION_SZ * 2 + 6, "Active: %c", g_status.active_color);
  mvwprintw(stdscr, 4, CKBD_DIMENSION_SZ * 2 + 6, "Steps : %d", g_status.steps);
  //更多状态信息
}

void show_winner(char *msg)
{
    mvwprintw(stdscr, 6, CKBD_DIMENSION_SZ * 2 + 6, "%s", msg);
    mvwprintw(stdscr,7, CKBD_DIMENSION_SZ * 2 + 6, "Press any key to push out", NULL);
}


//用法提示
void usage()
{ 
  printf("\nUsage:\n");
  printf("    (1)Host mode:\n");
  printf("       GoBangGame -h <tcp_port>\n");
  printf("    (2)Guest mode:\n");
  printf("       GoBangGame -g <host_ip> <tcp_port>\n\n");
}

bool is_victory(char color) {//判断对局是否胜利
    int i, j;
    //行判断
    for (i = 0; i < CKBD_DIMENSION_SZ; i++)
    {
        int sum = 0;
        for (j = 0; j < CKBD_DIMENSION_SZ; j++) {
            if (g_ckbd_situation[i][j] == color) {
                sum++;
                if (sum >= 5) {
                    return true;
                }
            }
            else {
                sum = 0;
            }
        }
    }
    //列判断
    for (i = 0; i < CKBD_DIMENSION_SZ; i++)
    {
        int sum = 0;
        for ( j = 0; j < CKBD_DIMENSION_SZ; j++) {
            if (g_ckbd_situation[j][i] == color) {
                sum++;
                if (sum >= 5)
                    return true;
            }
            else {
                sum = 0;
            }
        }

    }
    //斜线判断
    int diagonal[2 * CKBD_DIMENSION_SZ - 1] = { 0 };
    for (i = 0; i < CKBD_DIMENSION_SZ; i++) {
        for (j = 0; j < CKBD_DIMENSION_SZ; j++) {
            int index = i - j + CKBD_DIMENSION_SZ - 1;
            if (g_ckbd_situation[j][i] == color) {
                diagonal[index]++;
                if (diagonal[index] >= 5) {
                    return true;
                }
            }
            else {
                diagonal[index] = 0;
            }
        }
    }
    for (i = 0; i < 2 * CKBD_DIMENSION_SZ - 1; i++) {
        if (diagonal[i] >= 5) {
            return true;
        }
    }
    //斜对角线判断
    int back_diagonal[2 * CKBD_DIMENSION_SZ - 1] = { 0 };
    for (i = 0; i < CKBD_DIMENSION_SZ; i++) {
        for (j = 0; j < CKBD_DIMENSION_SZ; j++) {
            int index = i + j;
            if (g_ckbd_situation[j][i] == color) {
                back_diagonal[index]++;
                if (back_diagonal[index] >= 5) {
                    return true;
                }
            }
            else {
                back_diagonal[index] = 0;
            }
        }
    }
    return false;
}
//获取服务端监听套接字
int open_listen_sock() {
    	int sock = socket(AF_INET,SOCK_STREAM,0);
    	int cli_fd;
	if(0 > sock)
	{
		perror("socket");
		return -1;
	}
	

	struct sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(g_tcp_port);
	addr.sin_addr.s_addr = inet_addr("172.18.54.157");
	socklen_t addrlen = sizeof(addr);
	

	if(bind(sock,(struct sockaddr*)&addr,addrlen))
	{
		perror("bind");
		return -1;
	}
	
	if(listen(sock,50))
	{
		perror("listen");
		return -1;
	}
	for(;;)
	{

		struct sockaddr_in src_addr = {};
		cli_fd = accept(sock,(struct sockaddr*)&addr,&addrlen);
		return cli_fd;
	}

}
//获取客户端监听套接字
int open_client_sock() {
    struct hostent *hp;
	struct sockaddr_in serveraddr;
	//分配内存 创建socket对象 初始化地址 绑定 连接
	int nw=socket(AF_INET,SOCK_STREAM,0);
	if(nw<0){
		printf("error\n");
		return -1;
	}
	if((hp=gethostbyname(g_host_ip))==NULL)
		return -2;
	bzero((char *)&serveraddr,sizeof(serveraddr));
	serveraddr.sin_family=AF_INET;
	bcopy((char *)hp->h_addr_list[0],(char *)&serveraddr.sin_addr.s_addr,hp->h_length);
	serveraddr.sin_port=htons(g_tcp_port);
	
	if(connect(nw,(struct sockaddr*)&serveraddr,sizeof(serveraddr))<0)
		return -1;
    return nw;
}
void show_game_info(char color,char *msg) {
    bool re;
    re = is_victory(color);
    if (re) {
        show_winner(msg);
        wrefresh(stdscr);
        g_status.piece_color = 'E';
    }
}

void* netcom_thread_func()
{
    int ret;
    int sock;
    struct place_info step_info;

    if (g_host_flag == 1) {//主机通信操作
		sock = open_listen_sock();//获取服务端监听套接字
         while (1) {
            memset(&step_info, 0x00, sizeof(step_info));
            ret = recv(sock ,&step_info, sizeof(step_info),0);//等待客机落子
            
            g_ckbd_situation[step_info.y][step_info.x] = g_guest_piece_color; //将客机下的子设置到数组

            //收到坐标并且填充到棋盘后判断对方是否赢了
            show_game_info(g_guest_piece_color,"You failed!!!");

            memcpy(&g_step_info, &step_info,sizeof(step_info));//备份落子坐标
            ret = kill(getpid(),10);//触发信号量绑定函数，触发界面刷新

            g_status.active_color = g_host_piece_color;  //

            ret = sem_wait(&mutex);//等待本方界面落子
            memcpy(&step_info, &g_step_info, sizeof(step_info));//备份本方界面落子坐标
            ret = send(sock, &step_info,sizeof(step_info),0);//发送本方坐标


            //发送坐标后判断自己是否赢了
            show_game_info(g_host_piece_color, "You win!!!");

            g_status.active_color = g_guest_piece_color;  //
        }
    }
    else {//客机通信操作
		sock =open_client_sock();
         while (1) {
             g_status.active_color = g_guest_piece_color;

             ret = sem_wait(&mutex);
             int x, y;
             

             memcpy(&step_info, &g_step_info, sizeof(step_info));
             ret = send(sock, &step_info, sizeof(step_info), 0);//发送客机下的位置

             //发送坐标后判断自己是否赢了
             show_game_info(g_guest_piece_color, "You win!!!");

             g_status.active_color = g_host_piece_color;
             
             memset(&step_info, 0x00, sizeof(step_info));
             ret = recv(sock, &step_info, sizeof(step_info), 0);
             if (ret < 0) {
                 show_message("recv错误\n");
             }
             
             g_ckbd_situation[step_info.y][step_info.x] = g_host_piece_color;

             //收到坐标并且填充后判断对方是否赢了
             show_game_info(g_host_piece_color, "You failed!!!");

             memcpy(&g_step_info, &step_info, sizeof(step_info));
             ret = kill(getpid(), 10);
        }
    }
}


void handler1(int sig){
  show_message("调用了handler1\n");
  //printf("调用了handler\n");
  show_situation(g_step_info.x, g_step_info.y);  //显示棋局
  show_status(); //显示状态
 
}

//---------------------------------------------------
//主函数
int main(int argc,char* argv[])  
{ 
	int x, y;
	int cand_x, cand_y;
	int key = 0, quit = 0;
	int ret;

  //获取命令行参数
  memset(&g_status, 0x00, sizeof(g_status));
  memset(g_host_ip, 0x00, sizeof(g_host_ip));	//主机IP地址, 客方需要
	g_tcp_port = 0;    //主机TCP通信端口	 
  if(argc < 3) { //参数太少
    usage();
    exit(1);
  }

  if(strncmp(argv[1], "-h", 2) == 0) {  //主机模式
    g_tcp_port = atoi(argv[2]);  //TCP端口号
    g_status.piece_color = PIECE_WHITE_CH;  //主机执白
    g_host_piece_color = PIECE_WHITE_CH;  //主机棋子颜色
    g_host_flag = 1;
  }
  else if(strncmp(argv[1], "-g", 2) == 0) {  //来客模式
  	if(argc < 4) { //参数太少
      usage();
      exit(1);
    }
  	strncpy(g_host_ip, argv[2], sizeof(g_host_ip)-1);  	//客户机IP
    g_tcp_port = atoi(argv[3]);  //TCP端口号
    g_status.piece_color = PIECE_BLACK_CH;  //来宾执黑
    g_guest_piece_color = PIECE_BLACK_CH;  //客机棋子颜色
  }
  else { //参数无效
  	usage();
    exit(1);
  }

   //初始化信号量,初始值为0
  ret = sem_init(&mutex,0,0);
  if (ret != 0) {
      printf("sem_init ERROR!");
      exit(1);
  }

  //定义signal绑定函数 
  signal(SIGUSR1, handler1);

  //启动网络通信线程
  pthread_t t_id;
	pthread_create(&t_id,0,netcom_thread_func,NULL);
	setlocale(LC_ALL,"");
  //初始化ncurses界面
  initscr();    //初始化ncurses环境
  curs_set(0);  //不显示光标
  noecho();     //按键不回显
  cbreak();     //每次按键不缓存，getch()立即收到
  
	//初始化棋局与状态
	memset(g_ckbd_situation, POSITION_BLANK_CH, sizeof(g_ckbd_situation));
    cand_x = -1; //初始化待下棋子位置
	cand_y = -1;
	
  //设置测试状态
	
  //初始化待下棋子位置
	get_candidate_piece_pos(&cand_x, &cand_y, 0);
	
	show_situation(cand_x, cand_y);  //显示棋局
	show_status();   //显示状态
	show_message("Please choose a position to put your piece.");  //显示提示信息(测试)
	
	keypad(stdscr, TRUE);  //启动终端键盘
	
	while(1) {				
		
    key = getch();
    switch (key) {
    case 10: //回车
    case 13:
      if(g_status.active_color == g_status.piece_color) {  //本方下子
        g_ckbd_situation[cand_y][cand_x] = g_status.piece_color;  //在棋局矩阵中设置棋子状态
        g_status.steps++;
        g_step_info.x = cand_x;
        g_step_info.y = cand_y;
        g_step_info.stepes= g_status.steps;
        ret = sem_post(&mutex);
      }
      break;
    case KEY_LEFT:
    case KEY_RIGHT:
    case KEY_UP:
    case KEY_DOWN:
    	get_candidate_piece_pos(&cand_x, &cand_y, key);
			
		 //计算可下位置坐标
      break;
    default:
      mvprintw(CKBD_DIMENSION_SZ + 5, 4, "%d", key);
    }		
    show_situation(cand_x, cand_y);  //显示棋局
    show_status(); //显示状态
    show_message("Please choose a position to put your piece.");  //显示信息(测试)
  }
  endwin();
  return 0;
}
//---------------------------------------------------