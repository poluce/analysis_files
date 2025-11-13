# Git Hooks - 文档自动化工具

本目录包含用于自动化文档维护的 Git hooks。

## 可用的 Hooks

### pre-push.sample - 推送前文档检查

**功能**：
- 检查最近 3 次提交中是否更新了 CLAUDE.md
- 如果未更新，提醒用户确认是否需要更新文档
- 提供快速更新方法提示

**安装方法**：
```bash
# 复制到 .git/hooks 目录
cp .git-hooks/pre-push.sample .git/hooks/pre-push

# 添加执行权限
chmod +x .git/hooks/pre-push
```

**卸载方法**：
```bash
# 删除 hook
rm .git/hooks/pre-push
```

**跳过检查**：
如果某次推送确实不需要更新文档，在提示时输入 `y` 即可继续。

## 其他文档维护工具

### Slash Command: /update-docs

更推荐的方法是使用 `/update-docs` 命令：
- 自动分析最近的 git 提交
- 智能生成文档更新建议
- 一键更新项目统计数据

使用方法：
```
在 Claude Code 中输入：/update-docs
```

## 文档更新最佳实践

1. **完成功能后立即更新** - 趁记忆犹新时记录
2. **合并 PR 前检查** - 确保文档与代码同步
3. **使用自动化工具** - 减少手动工作量
4. **定期回顾** - 每周检查一次文档完整性

详见 `CLAUDE.md` 中的"文档维护指南"章节。
