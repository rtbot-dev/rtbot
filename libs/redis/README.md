  visitor.visitStart(rtbotParser::StartContext *context)
  # redis-rtbot

This is a native redis module that allows running `rtbot` programs
inside redis.

## Api

Commands are created following the redis convention that the first part
of the command should denote to which module they belong to.

### Commands

The following definitions are experimental and may change at any time.

#### `rtbot.run`

Given a program in yaml format stored inside redis at the key `pKey`,
an input timeseries at key `tsKey` and a prefix `prefix`, which is a
string that will be used to prefix output timeseries produced by the
program, send each one of the entries in the input timeseries to the
pipeline.

Example usage:

```redis
rtbot.run pKey tsKey prefix
```

The output of the program will appear in the timeseries key:

- `prefix:outputId1`
- ...
- `prefix:outputIdN`

where `outputIdi` is the id of the _i-th_ output operator.

TODO: Next version of the command would allow to pass multiple timeseries
as input:


```redis
rtbot.run pKey tsKey1 ... tsKeyM prefix
```