# Get new commits

通过libgit2仅获取项目更新的内容。

目前可以使用`git_odb_write`insert一个test object了。

TODO:
- [x] 遍历repo的object，全部insert到rocksdb中
- [x] 基于rocksdb验证能否实现增量更新
- [ ] 跑通git_odb_write_multi_pack_index,实现从pack文件读取object数据
- [ ] git_odb_open_wstream,据说比git_odb_write快

## 通过rocksdb转存，获取更新到独立pack文件
```shell
# 在bin文件目录中
# 1. 首先将地址中的git object保存在../tmp_rocks中
./gitforeach <repos_dir>
# 2. 然后删除repos中的.git/objects/pack

# 3. 执行定制化clone
./gitclone <repos_dir> <url>
```

# locate commit
定位commit最先出现的分支。

# 一些有用的debug 代码

```cpp
char shortsha[41] = {0};
git_oid_tostr(shortsha, 41, id);
printf("check for  %s\n", shortsha);
```

解析object可以参考[link](https://fuchsia.googlesource.com/third_party/libgit2/+/HEAD/examples/general.c)