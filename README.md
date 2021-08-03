# Publisher/Subscriber bus

Old test code for an abstract multithreaded publisher/subscriber model.

## How it works

The pub/sub model revolves around **events**, which each have a unique type and associated data. 

At the beginning of execution (or at any time), **subscribers** inform the pub-sub model of which **events** they care about.

During runtime, **events** are produced by other parts of the program, and **published**; i.e. added to the pub/sub buffer.

The buffer is emptied by multiple threads. When each **event** is thus processed, **subscribers** that subscribe to that event are given the **event**, along with its instance-specific data. These **subscribers** generally take the form of a function that is run on the **event**. Such a function could **publish** further **events**, which could be handled by other **subscribers**.

## Demo code particulars

The demo code is a standalone test that runs a handful of arbitrary subscribers. Upon execution, the user publishes several event(s) by providing a string to stdin. Each character in the string (in the range a-f) adds a particular event to the bus. Some events do nothing; others produce further events; others demonstrate "event data" which is associated with a specific event. See the file for details.
