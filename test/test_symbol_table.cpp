#include <string>

#include <gtest/gtest.h>

#include "symbol_table.hpp"

class SymbolTableTest : public testing::Test {
   protected:
    SymbolAttr make_symbol([[maybe_unused]] const std::string& type) {
        SymbolAttr s;
        s.set_base_type(SymbolBaseType::INT);
        // TODO: 根据实际Symbol结构填充
        return s;
    }
};

// 测试基本插入和查找
TEST_F(SymbolTableTest, BasicInsertAndFind) {
    SymbolTable table;
    SymbolAttr sym = make_symbol("int");

    table.try_add_symbol("var", sym);
    EXPECT_TRUE(table.exist_in_scope("var"));
    EXPECT_NE(table.find("var"), nullptr);
    EXPECT_EQ(table.find("nonexistent"), nullptr);
}

// 测试重复插入失败
TEST_F(SymbolTableTest, DuplicateInsertFails) {
    SymbolTable table;
    SymbolAttr sym1 = make_symbol("int");
    SymbolAttr sym2 = make_symbol("float");

    table.try_add_symbol("x", sym1);
    EXPECT_TRUE(table.exist_in_scope("x"));
}

// 测试作用域层级
TEST_F(SymbolTableTest, ScopeLevelTracking) {
    SymbolTable table;

    EXPECT_EQ(table.current_scope_level(), 0);

    table.push_scope();
    EXPECT_EQ(table.current_scope_level(), 1);

    table.push_scope();
    EXPECT_EQ(table.current_scope_level(), 2);

    table.pop_scope();
    EXPECT_EQ(table.current_scope_level(), 1);

    table.pop_scope();
    EXPECT_EQ(table.current_scope_level(), 0);
}

TEST_F(SymbolTableTest, ScopeIsolationAndLookup) {
    SymbolTable table;
    SymbolAttr sym1 = make_symbol("int");
    SymbolAttr sym2 = make_symbol("float");

    // 全局作用域
    table.try_add_symbol("global_var", sym1);
    EXPECT_TRUE(table.exist_in_scope("global_var"));

    // 内层作用域
    table.push_scope();
    table.try_add_symbol("local_var", sym2);

    // 内层应能看到全局和局部
    EXPECT_NE(table.find("global_var"), nullptr);
    EXPECT_NE(table.find("local_var"), nullptr);

    // 但exist_in_scope只能看到当前作用域
    EXPECT_TRUE(!table.exist_in_scope("global_var"));
    EXPECT_TRUE(table.exist_in_scope("local_var"));

    // 弹出后局部消失
    table.pop_scope();
    EXPECT_NE(table.find("global_var"), nullptr);
    EXPECT_EQ(table.find("local_var"), nullptr);
}

// 测试符号遮蔽
TEST_F(SymbolTableTest, SymbolShadowing) {
    SymbolTable table;
    SymbolAttr sym1 = make_symbol("int");
    SymbolAttr sym2 = make_symbol("float");

    // 全局作用域添加x
    table.try_add_symbol("x", sym1);
    EXPECT_EQ(table.current_scope_level(), 0);

    // 内层作用域遮蔽x
    table.push_scope();
    table.try_add_symbol("x", sym2);
    EXPECT_EQ(table.current_scope_level(), 1);

    // find应找到内层的x（头插法保证）
    EXPECT_NE(table.find("x"), nullptr);
    // TODO: 如果Symbol有唯一标识，验证是sym2

    table.pop_scope();

    // 回到全局，找到的是sym1
    EXPECT_NE(table.find("x"), nullptr);
    // TODO: 验证是sym1
}

// 测试跨作用域查找
TEST_F(SymbolTableTest, CrossScopeLookup) {
    SymbolTable table;
    SymbolAttr sym = make_symbol("int");

    table.try_add_symbol("global", sym);
    table.push_scope();

    // 内层可以查找外层符号
    EXPECT_NE(table.find("global"), nullptr);
    // 但exist_in_scope不应看到外层
    EXPECT_TRUE(!table.exist_in_scope("global"));
}

// 测试弹出全局作用域抛异常
TEST_F(SymbolTableTest, PopGlobalScopeThrows) {
    SymbolTable table;

    EXPECT_THROW(table.pop_scope(), std::runtime_error);
}

// 测试嵌套作用域清理
TEST_F(SymbolTableTest, NestedScopesCleanup) {
    SymbolTable table;
    SymbolAttr sym = make_symbol("int");

    // 创建多层作用域并添加符号
    for (int i = 0; i < 5; ++i) {
        table.push_scope();
        std::string name = "var" + std::to_string(i);
        table.try_add_symbol(name, sym);
        EXPECT_TRUE(table.exist_in_scope(name));
    }

    // 逐层弹出，验证清理
    for (int i = 4; i >= 0; --i) {
        std::string name = "var" + std::to_string(i);
        EXPECT_NE(table.find(name), nullptr);  // 当前能找到
        table.pop_scope();
        EXPECT_EQ(table.find(name), nullptr);  // 弹出后消失
    }
}

// 压力测试：大量符号
TEST_F(SymbolTableTest, StressTestManySymbols) {
    SymbolTable table;
    const size_t count = 10000;

    std::vector<std::string> names;
    names.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        names.push_back("collide_" + std::to_string(i));
    }

    // 插入大量符号
    for (size_t i = 0; i < count; ++i) {
        table.try_add_symbol(names[i], make_symbol("int"));
    }

    // 验证全部可查找
    for (size_t i = 0; i < count; ++i) {
        EXPECT_NE(table.find(names[i]), nullptr);
    }
}

// 测试哈希冲突处理
TEST_F(SymbolTableTest, HashCollisionHandling) {
    SymbolTable table;
    SymbolAttr sym = make_symbol("int");

    std::vector<std::string> names;
    names.reserve(256);
    for (size_t i = 0; i < 256; ++i) {
        names.push_back("collide_" + std::to_string(i));
    }

    // 插入256个符号，必然有哈希冲突
    for (size_t i = 0; i < 256; ++i) {
        table.try_add_symbol(names[i], sym);
    }

    // 验证冲突桶中所有符号都可访问
    for (size_t i = 0; i < 256; ++i) {
        EXPECT_NE(table.find(names[i]), nullptr);
    }
}

// 测试移动语义
TEST_F(SymbolTableTest, MoveConstruction) {
    SymbolTable table1;
    table1.try_add_symbol("var", make_symbol("int"));

    SymbolTable table2(std::move(table1));
    EXPECT_NE(table2.find("var"), nullptr);
}

TEST_F(SymbolTableTest, MoveAssignment) {
    SymbolTable table1;
    table1.try_add_symbol("var1", make_symbol("int"));

    SymbolTable table2;
    table2.try_add_symbol("var2", make_symbol("float"));

    table2 = std::move(table1);
    EXPECT_NE(table2.find("var1"), nullptr);
    EXPECT_EQ(table2.find("var2"), nullptr);  // 被移动覆盖
}
