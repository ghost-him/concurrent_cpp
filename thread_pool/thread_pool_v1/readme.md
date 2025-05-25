做了两个次不同的实验。首先是对于少数的，但运行时间长的测试：

```
Benchmark Configuration:
------------------------
Number of tasks: 10000
Task complexity (iterations): 500000
Hardware concurrency: 12
------------------------

Running test WITH Thread Pool...
Thread Pool test finished.

Running test WITHOUT Thread Pool (using std::jthread)...
std::jthread test finished.

--- Benchmark Results ---
Time with Thread Pool:    1172.884 ms
Time with std::jthread:   1260.462 ms
Results from both methods match.
```

可以看到线程池只快一点点

而当运行的任务为时间短，但是频繁调用时，效果就不错了：

```
Benchmark Configuration:
------------------------
Number of tasks: 500000
Task complexity (iterations): 10000
Hardware concurrency: 12
------------------------

Running test WITH Thread Pool...
Thread Pool test finished.

Running test WITHOUT Thread Pool (using std::jthread)...
std::jthread test finished.

--- Benchmark Results ---
Time with Thread Pool:    1212.552 ms
Time with std::jthread:   9724.814 ms
Results from both methods match.
```

以上两个实验都是cpu密集型的任务。在v2时，会将线程池做优化，可以让用户手动输入这个任务是io密集型还是cpu密集型。对于不同类型的任务，线程池会给出不同的处理逻辑。