#include "duckdb.hpp"
#include <iostream>

using namespace duckdb;
using namespace std;

int main() {
    DuckDB db(nullptr);
    Connection con(db);
    
    // Test multiple statements in one Query call
    string sql = R"(
        CREATE OR REPLACE MACRO test1() AS ('first');
        CREATE OR REPLACE MACRO test2() AS ('second');
    )";
    
    auto result = con.Query(sql);
    if (result->HasError()) {
        cout << "Error: " << result->GetError() << endl;
    } else {
        cout << "Query executed successfully" << endl;
    }
    
    // Check which macros exist
    auto check1 = con.Query("SELECT test1()");
    auto check2 = con.Query("SELECT test2()");
    
    cout << "test1() exists: " << (check1->HasError() ? "NO" : "YES") << endl;
    cout << "test2() exists: " << (check2->HasError() ? "NO" : "YES") << endl;
    
    return 0;
}