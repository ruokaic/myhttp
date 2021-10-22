
**权限更改：**
有执行权限将会视为cgi文件被执行，应确保httpdocs目录下只有cgi文件有执行权限，其余文件没有：

sudo chmod 600 test.html

sudo chmod 600 post.html

sudo chmod +X post.cgi

**编译：**
输入make直接编译

**运行服务器：**
输入./myhttp

**测试:**
本机使用webbench模拟1000个客户端发送get 来测试服务器，结果如下：
 ![image](https://github.com/ruokaic/myhttp/blob/main/picture/并发压力测试.png)  
**测试过程浏览器可流畅打开页面**  
1、浏览器测试get方法：默认端口号是8888，在浏览器输入本地IP：192.168.xxx.xxx:8888 成功则跳转到test.html界面
 ![image](https://github.com/ruokaic/myhttp/blob/main/picture/test1.png)
 
2、浏览器测试post方法：浏览器输入192.168.xxx.xxx:8888/post.html跳转到输入界面，填写内容，点击提交，成功则会返回填写内容
 ![image](https://github.com/ruokaic/myhttp/blob/main/picture/test2.png)
 ![image](https://github.com/ruokaic/myhttp/blob/main/picture/test3.png)

