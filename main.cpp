#include <iostream>
#include <stdio.h>
#include <string.h>

#include <ThreadPool.h>

void func1(int arg)
{
    std::cout << arg << std::endl;
}

void func2(int arg)
{
    std::cout << arg << std::endl;
}

void func3(int arg)
{
    std::cout << arg << std::endl;
}

void func4(int arg)
{
    std::cout << arg << std::endl;
}

int main(int argc, char **argv)
{
    ThreadPool thp(4);

    thp.addTask(func1, 1);
    thp.addTask(func2, 2);

    int i = 0;
    while(i < 100)
    {
        thp.addTask(func1, 1);
        thp.addTask(func2, 2);
        thp.addTask(func3, 3);
        thp.addTask(func4, 4);
        std::this_thread::sleep_for(std::chrono::seconds(1));
        ++i;
    }
    std::cout << "over" << std::endl;
}