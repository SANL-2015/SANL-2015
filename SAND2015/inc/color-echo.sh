#!/bin/bash
# color-echo.sh: 使用颜色来显示文本消息.

# 可以按照你自己的目的来修改这个脚本.
# 这比将颜色数值写死更容易.

black='\E[30;47m'
red='\E[31;47m'
green='\E[32;47m'
yellow='\E[33;47m'
blue='\E[34;47m'
magenta='\E[35;47m'
cyan='\E[36;47m'
white='\E[37;47m'


alias Reset="tput sgr0"      #  不用清屏,
                             #+ 将文本属性重置为正常情况.


cecho ()                     # Color-echo.
                             # 参数$1 = 要显示的信息
                             # 参数$2 = 颜色
{
local default_msg="No message passed."
                             # 其实并不一定非的是局部变量.

message=${1:-$default_msg}   # 默认为default_msg.
color=${2:-$black}           # 如果没有指定, 默认为黑色.

  echo -e "$color"
  echo "$message"
  #reset                      # 重置文本属性.

  return
}
