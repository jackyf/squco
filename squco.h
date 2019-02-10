#pragma once

#include <QEvent>
#include <QAbstractEventDispatcher>
#include <QCoreApplication>
#include <QPointer>

namespace squco {
namespace detail {

template <typename F>
struct FEvent : public QEvent {
	using Fun = typename std::decay<F>::type;
	Fun fun;
	FEvent(Fun && fun) : QEvent(QEvent::None), fun(std::move(fun)) {}
	FEvent(const Fun & fun) : QEvent(QEvent::None), fun(fun) {}
	~FEvent() { fun(); }
};

template <typename GuardT, typename SlotT>
void postGuarded(QThread* th, GuardT guard, SlotT&& slot) {
	auto* ed = QAbstractEventDispatcher::instance(th);
	if (Q_UNLIKELY(!ed)) {
		qWarning("squco: no event dispatcher available for thread %p", th);
		return;
	}

	auto guardedSlot = [guard, slot=std::move(slot)]{
		if (guard) {
			slot();
		}
	};
	QCoreApplication::postEvent(ed,
			new FEvent<decltype(guardedSlot)>(std::move(guardedSlot)));
}

}

template <typename SignallerT, typename SignalT, typename SlotT>
QMetaObject::Connection connectFunctor(SignallerT from, SignalT sig, QObject* anchor, SlotT&& f) {
	return QObject::connect(from, sig,
			[th=anchor->thread(), guard=QPointer<QObject>(anchor), f=std::move(f)](auto&&... args) {
				detail::postGuarded(th, guard, [=]{ f(args...); });
			});
}

template <typename SignallerT, typename SignalT, typename TargetT, typename... SlotArgsT>
QMetaObject::Connection connectSlot(SignallerT* from, SignalT sig, TargetT* target, void (TargetT::*slot)(SlotArgsT...)) {
	return connectFunctor(from, sig, target, [target, slot](auto&&... args) { (target->*slot)(args...); });
}

}
