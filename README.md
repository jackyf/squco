What is it?
-----------
**S**tatic **qu**eued **co**nnections for Qt signals.


Why?
----

- arguments can be any copyable types, i.e.:
  - no type registration needed (`Q_DECLARE_METATYPE`, `qRegisterMetaType()` etc.);
  - no default constructors required;
- slightly faster (no per-argument memory allocations);
- argument types in signal declarations do not need to be namespace-qualified.


Why not?
--------

- receivers must not change their thread affinity while any `squco`-connections any active,
  undefined behaviour otherwise;
- error messages on incompatible parameters may be worse than from `QObject::connect()`;


Requirements
------------

- C++14;
- Qt5 (tested on Qt 5.9.1)


Usage
-----

Similar to the standard 4-argument `QObject::connect()`:

To connect:

`squco::connectSlot(f, &From::signalX, t, &To::slotY);`

`squco::connectFunctor(f, &From::signalX, t, [t, otherParam]{ t->abc(5, otherParam); });`

To disconnect:

```
auto conn = squco::connectSlot(...);
...
QObject::disconnect(conn);
```

Naturally, `squco` always assumes Qt::QueuedConnection. 

**Not yet tested in production.** Reports welcome.


More examples
-------------

See unit tests in `test.cpp`.


License
-------

MIT
