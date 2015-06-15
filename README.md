# logger
php打印日志扩展
使用方法
logger::init(app_name, path);

logger::trace("this is my first log");

一般在index入口文件使用logger::init(app_name, path);


在需要记录日志的地方使用

logger::trace("this is my first log");

日志格式打印出来为

[trace][2015-6-15 22:26:9][当前php文件全路径][当前内存使用量] this is my first log


使用前请生成path/app_name 路径

本扩展为本人第一个扩展，以后将不断的完善和加强功能，有一些问题请联系我 Email : yzx753@vip.qq.com
