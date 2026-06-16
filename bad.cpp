#include <iostream>
#include <string>
#include <vector>
#include <thread>


using std::cout;
using std::cin;
using std::endl;


void test01(){
    int n = 5;
    int* a = new int[n]{1,2,3};
    for(int i = 0; i< n ;i++){
        cout << a[i] << " ";
    }
    cout << endl;
    
    delete a;

}

// 全局共享变量（无任何保护）
int count = 0;
// 高并发下不安全的函数
void unsafe_increment() {
    for (int i = 0; i < 100000; ++i) {
        // 关键漏洞点：count++ 不是原子操作
        // 分为三步：读 → 加1 → 写回
        // 多线程同时执行会互相覆盖
        count++;
    }
}
void test02() {
    const int thread_num = 10;
    std::vector<std::thread> threads;

    // 创建多个线程同时修改共享变量
    for (int i = 0; i < thread_num; ++i) {
        threads.emplace_back(unsafe_increment);
    }

    // 等待所有线程结束
    for (auto& t : threads) {
        t.join();
    }

    // 理论结果应该是 10 * 100000 = 1000000
    // 实际运行每次都不一样，远小于 1000000
    cout << "最终 count = " << count << endl;
}

void test03(){

    int* data = new int[100];

    data[0] = 123;
    cout << data[0] << endl;
    delete[] data;

    data[0] = 456;
    cout << data[0] << endl;

    int* ptr = nullptr;
    cout << *ptr << endl;
}

int main(){
    test01();
    test02();
    test03();
}
