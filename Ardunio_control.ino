#include <Wire.h>
#include <Servo.h>  // 添加舵机库

Servo myservo;         // 定义舵机1
int servo = 3;         // 舵机引脚
int datum_angle = 84;  // 直行基准角
int right_angle = 106;
int left_angle = 52;
int if_break = 1, start = 0, working = 0, arrive = 0, if_return = 0, error = 0;
int place_num = 0;
int num = 0;

#define SLAVE_ADDRESS 0x08  // OpenMV 的 I2C 地址
#define PWMA 5              //电机A控制端
#define PWMB 6              //电机B控制端

void setup() {
  Wire.begin();                // 初始化 I2C 作为主机
  Serial.begin(9600);          // 初始化串口通信，波特率 9600
  init_servo(myservo, servo);  // 舵机
  pinMode(PWMA, OUTPUT);       // 电机
  pinMode(PWMB, OUTPUT);
  control_turn(myservo, datum_angle);
}

void loop() {
  //urat连接华为板  -->  if_break ：没收到任务
  //urat连接华为板  -->  start ： 1 之后 就变 0
  //urat连接华为板  -->  working ：正在运行
  //urat连接华为板  -->  arrive & braek ：arrive waiting
  //urat连接华为板  -->  if_return   返回
  //urat连接华为板  -->  error    遇到障碍   error大于50报错

  send_state(if_break, start, working, arrive, if_return, error);

  if (if_break == 1) {
    // 获取目的地点
    place_num = route_select();
    if (place_num != 0) {
      start = 1;
      working = 1;
      if_break = 0;
    }
  }

  // 工作
  if (working == 1 && error != 2) {
    if (start == 1) {
      // 发送 "start" 字符串到 OpenMV
      Wire.beginTransmission(SLAVE_ADDRESS);  // 开始传输
      Wire.write("star");                     // 发送字符串 "star"
      Wire.endTransmission();                 // 结束传输
      start = 0;
      delay(10);  // 等待从机处理数据
    } else {
      Wire.beginTransmission(SLAVE_ADDRESS);  // 开始传输
      Wire.write("work");                     // 发送字符串 "work"
      Wire.endTransmission();                 // 结束传输
      delay(10);                              // 等待从机处理数据
    }
    Wire.requestFrom(SLAVE_ADDRESS, 4);  // 请求从机发送 4 字节的数据

    // 缓冲区用于存储接收到的数据 "line" 长度为 4，加上终止符 '\0' 的缓冲区长度为 5 （cros（cross），stop）
    char receivedData[6];
    int index = 0;

    // 读取接收到的数据
    while (Wire.available()) {
      char c = Wire.read();  // 从 I2C 读取一个字符
      if (index < 5) {
        receivedData[index++] = c;  // 存储接收到的字符
      }
    }
    receivedData[index] = '\0';  // 添加终止符

    // 比较接收到的数据 输出 0停止  1前进  2左转  3右转  6arrive，均会自动运动; 输入是：stop、line、cros、其他
    int re = receiv_reaction(receivedData, place_num);

    switch (re) {
      case 1:
        error = 0;
        break;
      case 2:
        error = 0;
        break;
      case 3:
        error = 0;
        break;
      case 6:
        working = 0;
        arrive = 1;
        error = 0;
        break;
      case 7:
        error = 2;
        break;
      default:
        error = 1;
        break;
    }
  }

  // 返回工作  没写！！！
  if (arrive > 0 && arrive < 100) {
    arrive++;
    delay(40);
  }
  // home要改成对应数字
  if (arrive == 100 && place_num != 4) {
    place_num = 4;
    if_return = 1;
    working = 1;
    arrive = 0;
    start_arri();
    //int if_return_arri = start_arri();
  }

  if (arrive == 100 && place_num == 4) {
    if_break = 1;
    start = 0;
    working = 0;
    arrive = 0;
    if_return = 0;
    error = 0;
    place_num = 0;
    start_arri();
    Serial.end();
    delay(100); // 给串口关闭一些时间
    Serial.begin(9600); // 使用你原本设置的波特率
  }
  delay(10);
}



//----------------------------------------------------------------------------------------------------------------------------------

//发送当前状态
void send_state(int if_break, int start, int working, int arrive, int if_return, int error) {
  int a_state = determine_status(if_break, start, working, arrive, if_return, error);
  Serial.print(a_state);
}

//接受路线指令
int route_select() {
  int route = 0;
  while (Serial.available() > 0) {
    String str = Serial.readString();
    route = str.toInt();
  }
  return route;
}

// 没写！！调头加启动回家 目的home
int start_arri() {
  control_CPR(0);
  delay(2000);

  control_turn(myservo, 110);
  control_CPR(1);
  delay(3950);

  control_CPR(0);
  delay(2000);

  control_turn(myservo, 84);
  control_CPR(2);
  delay(2000);

  control_CPR(0);
  delay(2000);

  control_turn(myservo, 110);
  control_CPR(1);
  delay(3800);

  control_turn(myservo, 84);
  control_CPR(0);
  delay(2000);
}


// 根据输入组合来确定状态的函数
int determine_status(int if_break, int start, int working, int arrive, int if_return, int error) {
  if (if_break == 1 && start == 0 && working == 0 && arrive == 0 && if_return == 0 && error == 0) {
    // 休息
    return 0;
  } else if (if_break == 0 && start == 0 && working == 1 && arrive == 0 && if_return == 0 && error == 0) {
    // 工作
    return 1;
  } else if (if_break == 0 && start == 0 && working == 0 && arrive > 1 && if_return == 0 && error == 0) {
    // 到达
    return 2;
  } else if (if_break == 0 && start == 0 && working == 1 && arrive == 0 && if_return == 1 && error == 0) {
    // 返回
    return 3;
  } else if (error > 0) {
    // 错误 遇到障碍
    return 4;
  } else {
    // 未知状态
    return -1;
  }
}

// 比较接收到的数据 0停止  1前进  2左转  3右转  6arrive 7 cros error
int receiv_reaction(char receivedData[6], int place_num) {

  if (strcmp(receivedData, "cros") == 0) {
    control_CPR(0);
    int cr_reaction = cross_reaction(place_num);
    return cr_reaction;
  }

  if (strcmp(receivedData, "stop") == 0) {
    control_CPR(0);
    control_turn(myservo, datum_angle);
    return 0;
  }

  // 中心在左，需要向左偏
  if (strcmp(receivedData, "lin1") == 0) {
    control_CPR_adjust(1, 4, myservo);
    return 1;
  }

  // 中心在右，需要向右偏
  if (strcmp(receivedData, "lin2") == 0) {
    control_CPR_adjust(1, 0, myservo);
    return 1;
  }

  if (strcmp(receivedData, "lin0") == 0) {
    control_CPR_str(1, num, myservo);
    return 1;
  } else {
    control_CPR(0);
    return 0;
  }
}

// cross判断  现在place_num只为1
// 输出 0停止  1前进  2左转  3右转  6arrive 7 cros error
int cross_reaction(int place_num) {
  // 开始传输
  Wire.beginTransmission(SLAVE_ADDRESS);
  switch (place_num) {
    case 4:
      Wire.write("home");
      break;
    case 1:
      Wire.write("croA");
      break;
    case 2:
      Wire.write("croB");
      break;
    case 3:
      Wire.write("croC");
      break;

    default:
      Wire.write("none");
      break;
  }
  Wire.endTransmission();  // 结束传输
  delay(100);              // 等待从机处理数据

  Wire.requestFrom(SLAVE_ADDRESS, 4);  // 请求从机发送 4 字节的数据
  char rD[6];
  int index = 0;

  // 读取接收到的数据
  while (Wire.available()) {
    char c = Wire.read();  // 从 I2C 读取一个字符
    if (index < 5) {
      rD[index++] = c;  // 存储接收到的字符
    }
  }
  rD[index] = '\0';  // 添加终止符
  // 输出 0停止  1前进  2左转  3右转  6arrive
  if (strcmp(rD, "goon") == 0) {
    control_CPR_str(1, num, myservo);
    delay(4000);
    return 1;
  }

  if (strcmp(rD, "arri") == 0) {
    control_CPR(0);
    return 6;
  }

  if (strcmp(rD, "left") == 0) {
    turnleft();
    return 2;
  }

  if (strcmp(rD, "righ") == 0) {
    turnright();
    return 3;
  } else
    control_CPR(0);
  return 7;
}

void init_servo(Servo myservo, int pin) {
  myservo.attach(pin);  // 转向舵机引脚
}

void control_turn(Servo myservo, int angle) {
  myservo.write(angle);
}

// 0停止  1前进  2后退
void control_CPR(int state) {
  switch (state) {
    case 1:
      digitalWrite(PWMA, LOW);
      analogWrite(PWMB, 155);
      Serial.println("cpr1");
      break;
    case 2:
      analogWrite(PWMA, 155);
      digitalWrite(PWMB, LOW);
      break;
    case 0:
      digitalWrite(PWMA, LOW);
      digitalWrite(PWMB, LOW);
      break;
    default:
      digitalWrite(PWMA, LOW);
      digitalWrite(PWMB, LOW);
      break;
  }
}

void control_CPR_str(int state, int num, Servo myservo) {
  switch (state) {
    case 1:
      digitalWrite(PWMA, LOW);
      analogWrite(PWMB, 90);
      if (num < 3) {
        control_turn(myservo, 84);
      } else if (num < 8) {
        control_turn(myservo, 83);
      }
      num++;
      if (num >= 8) {
        num = 0;
      }
      break;
    case 2:
      analogWrite(PWMA, 80);
      digitalWrite(PWMB, LOW);
      break;
    case 0:
      digitalWrite(PWMA, LOW);
      digitalWrite(PWMB, LOW);
      break;
    default:
      digitalWrite(PWMA, LOW);
      digitalWrite(PWMB, LOW);
      break;
  }
}

void control_CPR_adjust(int state, int num, Servo myservo) {
  switch (state) {
    case 1:
      digitalWrite(PWMA, LOW);
      analogWrite(PWMB, 90);
      if (num < 4) {
        control_turn(myservo, 90);
      } else if (num < 8) {
        control_turn(myservo, 77);
      }
      num++;
      if (num >= 8) {
        num = 0;
      }
      break;
    case 2:
      analogWrite(PWMA, 80);
      digitalWrite(PWMB, LOW);
      break;
    case 0:
      digitalWrite(PWMA, LOW);
      digitalWrite(PWMB, LOW);
      break;
    default:
      digitalWrite(PWMA, LOW);
      digitalWrite(PWMB, LOW);
      break;
  }
}

// 转弯后继续前进
void turnleft() {
  control_CPR(0);
  delay(10);
  control_turn(myservo, left_angle);
  control_CPR(1);
  delay(2600);
  control_CPR_str(1, num, myservo);
  delay(1600);
}

// 转弯后继续前进
void turnright() {
  control_CPR(0);
  delay(10);
  control_turn(myservo, right_angle);
  control_CPR(1);
  delay(3000);
  control_CPR_str(1, num, myservo);
  delay(1200);
}
