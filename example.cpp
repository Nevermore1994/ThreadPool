#include <iostream>
#include <vector>
#include <chrono>

#include "ThreadPool.h"

int calc(int x)
{
	if (x <= 2)
		return 1;
	return calc(x - 1) + calc(x - 2);
}


int main(int argc, char* argv[]) {
	ThreadPool p(3);
	auto res = p.Enqueue([ch = 'A']{
		for (int i = 0; i < 10; i++)
		{
			cout << ch << endl;
		}
	return ch;
	});
	cout << "main :" << res.get() << endl;
	p.Enqueue(print, 'B');
	p.Enqueue(print, 'C');
	auto t = p.Enqueue(calc, 10);
	cout << "calc : " << t.get() << endl;
	return 0;
}

