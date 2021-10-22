
权限更改：
有执行权限将会视为cgi文件被执行，应确保httpdocs目录下只有cgi文件有执行权限，其余文件没有：

sudo chmod 600 test.html

sudo chmod 600 post.html

sudo chmod +X post.cgi

编译：
输入make直接编译

运行服务器：
输入./myhttp


使用:

测试get方法：默认端口号是8888，在浏览器输入本地IP：192.168.xxx.xxx:8888 成功则跳转到test.html界面
测试post方法：浏览器输入192.168.xxx.xxx:8888/post.html跳转到输入界面，填写内容，点击提交，成功则会返回填写内容


