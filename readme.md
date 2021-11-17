# Background

Often on a shared service resource a customer can generate high CPU usage if their application can generate reports, this can impact other customers who generally only cause spike traffic.

Niced will monitor a process by name and adjust the NICE value if the process goes over a set CPU % for a duration.

Please note this is currently in beta, a lot will change over the development.

# Configuration

Currently the config file has 5 options.

## ProcessName

This is the name of the process you wish to monitor, for example if the process name is "MyProcess" it will be shown as the following in the config file:

```
ProcessName=MyProcess
```

## ThreshHold

CPU % Threshold, the example below will start the delay if the process uses over 50% CPU.

```
ThreshHold=50
```

## ThreshHoldDelay

How many second to wait while the CPU is above ThresHold before adjusting the NICE value, the below example is 5 seconds

```
ThreshHoldDelay=5
```

## NICEDefault

The NICE value of a process which is not being throttled, standard operating NICE value

```
NICEDefault=0
```

## NICEThrottled

The NICE value of a throttled process

```
NICEThrottled=1
```

# TODO

- Rename Config File field name
- Build Install Scripts
- Build INIT scripts
- Change from 1000 Entry Array to Dynamic array