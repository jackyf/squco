#include "squco.hpp"

#include <QTest>
#include <QThread>
#include <QTimer>

#include <memory>

struct A {
	int x;
	int y;
};

Q_DECLARE_METATYPE(A);

class CopyCounter {
public:
	explicit CopyCounter(int* counter) : m_counter(counter) {}
	CopyCounter(const CopyCounter& other) : m_counter(other.m_counter) {
		++*m_counter;
	}
	CopyCounter(CopyCounter&&) = default;

private:
	int* m_counter;
};

class C: public QObject {
	Q_OBJECT
signals:
	void reset();
	void s0();
	void s1(A);
	void s2(A, A);
	void s3(A, A, A);
	void cc(CopyCounter);
};

class D: public QObject {
	Q_OBJECT
public:
	D(int* data) : m_data(data) {}

public slots:
	void t0() {
		++*m_data;
	}
	void t1(const A& a) {
		*m_data += (a.x - a.y);
	}
	void t2(const A& a1, const A& a2) {
		*m_data += (a1.x - a2.y);
	}
	void t3(const A& a1, const A& a2, const A& a3) {
		*m_data += (a1.x - a2.y - a3.x);
	}
	void tcc(const CopyCounter&) {}

signals:
	void pong();

private:
	int* m_data;
};

class Test: public QObject {
	Q_OBJECT

private slots:
	void emitWhenAlive();
	void emitWhenDead();
	void toDifferentThread();
	void onlyOneCopy();
	void implicitNamespaceArgument();
	void disconnection();
	void bench_data();
	void bench();
};

void Test::emitWhenAlive() {
	int dData = 0;
	std::unique_ptr<D> d(new D(&dData));

	C c;
	squco::connectFunctor(&c, &C::s0, d.get(), [d=d.get()]{ d->t0(); });
	squco::connectFunctor(&c, &C::reset, d.get(), [&d]{ d.reset(); });

	for (int i = 0; i < 20; ++i) {
		if (i == 17) {
			emit c.reset();
		} else {
			emit c.s0();
		}
	}
	QCoreApplication::processEvents();
	QCOMPARE(dData, 17);
}

void Test::emitWhenDead() {
	int dData = 0;
	std::unique_ptr<D> d(new D(&dData));

	C c;
	squco::connectFunctor(&c, &C::s0, d.get(), [d=d.get()]{ d->t0(); });

	emit c.s0();
	d.reset();
	emit c.s0();
	QCoreApplication::processEvents();
	emit c.s0();

	QCoreApplication::processEvents();
	QCOMPARE(dData, 0);
}

template<typename TriggerT, typename ObjectT, typename SignalT>
void waitForSignal(TriggerT trigger, ObjectT* o, SignalT sig) {

	QEventLoop el;
	QObject::connect(o, sig, &el, &QEventLoop::quit);
	QTimer::singleShot(0, trigger);
	el.exec();
}

void Test::toDifferentThread() {
	QThread th;

	int dData = 0;
	auto* d = new D(&dData);
	d->moveToThread(&th);

	C c;
	squco::connectSlot(&c, &C::s1, d, &D::t1);
	squco::connectFunctor(&c, &C::s0, d, [d, &th, &dData]{
		dData *= 5;
		QCOMPARE(QThread::currentThread(), &th);
		emit d->pong();
		delete d;
	});

	waitForSignal([&th]{ th.start(); }, &th, &QThread::started);
	QVERIFY(th.eventDispatcher());

	emit c.s1({12, 9});
	waitForSignal([&c]{ emit c.s0(); }, d, &D::pong);

	th.quit();
	th.wait();
	QCOMPARE(dData, 15);
}

void Test::onlyOneCopy() {
	int dData = 0;
	std::unique_ptr<D> d(new D(&dData));

	C c;
	int c1 = 0, c2 = 0;

	squco::connectFunctor(&c, &C::cc, d.get(), [d=d.get()](const CopyCounter& cc) { d->tcc(cc); });
	emit c.cc(CopyCounter(&c1));
	QCoreApplication::processEvents();
	QCOMPARE(c1, 1);

	squco::connectSlot(&c, &C::cc, d.get(), &D::tcc);
	emit c.cc(CopyCounter(&c2));
	QCoreApplication::processEvents();
	QCOMPARE(c2, 2);
}

namespace ina {

struct St: public A {
	St(A a) : A(a) {}
};

class Emitter: public QObject {
	Q_OBJECT
signals:
	void s(St);
};

}

void Test::implicitNamespaceArgument() {
	using namespace ina;

	int result = 0;

	Emitter e;
	squco::connectFunctor(&e, &Emitter::s, &e, [&result](St st){ result = st.x*st.y; });

	emit e.s(A{2, 5});
	QCoreApplication::processEvents();
	QCOMPARE(result, 10);
}

void Test::disconnection() {
	int dData = 0;
	D d(&dData);

	C c;
	auto conn = squco::connectSlot(&c, &C::s1, &d, &D::t1);
	emit c.s1({19,15});
	QCoreApplication::processEvents();
	QCOMPARE(dData, 4);

	disconnect(conn);
	emit c.s1({10,9});
	QCoreApplication::processEvents();
	QCOMPARE(dData, 4); // not changed
}

void Test::bench_data() {
	QTest::addColumn<bool>("squco");
	QTest::addColumn<int>("argc");

	QTest::newRow("qt, 0") << false << 0;
	QTest::newRow("qt, 1") << false << 1;
	QTest::newRow("qt, 2") << false << 2;
	QTest::newRow("qt, 3") << false << 3;
	QTest::newRow("squco, 0") << true << 0;
	QTest::newRow("squco, 1") << true << 1;
	QTest::newRow("squco, 2") << true << 2;
	QTest::newRow("squco, 3") << true << 3;
}

void Test::bench() {
	QFETCH(bool, squco);
	QFETCH(int, argc);

	int dData;
	std::unique_ptr<D> d(new D(&dData));

	C c;
	if (squco) {
		squco::connectFunctor(&c, &C::s0, d.get(), [d=d.get()]{ d->t0(); });
		squco::connectFunctor(&c, &C::s1, d.get(), [d=d.get()](const A& a) { d->t1(a); });
		squco::connectSlot(&c, &C::s2, d.get(), &D::t2);
		squco::connectSlot(&c, &C::s3, d.get(), &D::t3);
	} else {
		connect(&c, &C::s0, d.get(), &D::t0, Qt::QueuedConnection);
		connect(&c, &C::s1, d.get(), &D::t1, Qt::QueuedConnection);
		connect(&c, &C::s2, d.get(), &D::t2, Qt::QueuedConnection);
		connect(&c, &C::s3, d.get(), &D::t3, Qt::QueuedConnection);
	}

	QBENCHMARK {
		dData = 0;
		for (int i = 0; i < 1000; ++i) {
			switch (argc) {
				case 0:
					emit c.s0();
					break;
				case 1:
					emit c.s1({20, 18});
					break;
				case 2:
					emit c.s2({10, 0}, {0, 7});
					break;
				case 3:
					emit c.s3({6, 0}, {0, 1}, {1, 0});
					break;
			}
		}
		QCoreApplication::processEvents();
		QCOMPARE(dData, 1000*(argc+1));
	}
}

QTEST_GUILESS_MAIN(Test);

#include "test.moc"
