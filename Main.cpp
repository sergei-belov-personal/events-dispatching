#include <algorithm>
#include <cstdint>
#include <functional>
#include <iostream>
#include <vector>

#if defined(_MSC_VER)
	// Microsoft compiler only.
	#define Assert(InExpression) if (!(InExpression)) __debugbreak()
#else
	#define Assert(InExpression) if (!(InExpression)) ((void*)0)
#endif


class TrackingObject_ final
{
public:
	TrackingObject_()
	{
		std::cout << "TrackingObject_();" << std::endl;
	}

	~TrackingObject_()
	{
		std::cout << "~TrackingObject_()" << std::endl;
	}

	TrackingObject_(const TrackingObject_&)
	{
		std::cout << "TrackingObject_(const TrackingObject_&)" << std::endl;
	}

	TrackingObject_(TrackingObject_&&) noexcept
	{
		std::cout << "TrackingObject_(TrackingObject_&&) noexcept" << std::endl;
	}

	TrackingObject_& operator=(const TrackingObject_&)
	{
		std::cout << "TrackingObject_& operator=(const TrackingObject_&)" << std::endl;
		return *this;
	}

	TrackingObject_& operator=(TrackingObject_&&) noexcept
	{
		std::cout << "TrackingObject_& operator=(TrackingObject_&&) noexcept" << std::endl;
		return *this;
	}
};

template <typename EventType_>
class Event_ final
{
	enum class EventState_ : int8_t
	{
		Waiting,
		Sending,
		Interrupted
	};

	enum class ReceiverState_ : int8_t
	{
		Active,
		Paused,
	};

	using ReceiverIndex_ = size_t;
	using ReceiverFunction_ = std::function<void(EventType_& InEvent)>;

	class ReceiverData_;

public:
	class Subscription_;
	
	template <typename SendEventType_ = EventType_, std::enable_if_t<std::is_convertible_v<SendEventType_, EventType_>, bool> = true>
	static void Send(SendEventType_&& InEvent)
	{
		if (State != EventState_::Waiting)
		{
			// Attempt to send event recursively;
			Assert(false);

			return;
		}

		State = EventState_::Sending;

		for (const auto& Receiver : ReceiversContainer)
		{
			if (State != EventState_::Sending)
			{
				// When sending is interrupted.
				break;
			}

			if (Receiver.State != ReceiverState_::Active)
			{
				// When receiver is on pause.
				continue;
			}

			Receiver.Function(InEvent);
		}

		State = EventState_::Waiting;
	}

	template<typename ... Types_>
	static void Send(Types_&& ... InArguments)
	{
		Send<EventType_>({std::forward<Types_>(InArguments)...});
	}

	template <typename ReceiverType_ = ReceiverFunction_, std::enable_if_t<std::is_convertible_v<ReceiverType_, ReceiverFunction_>, bool> = true>
	static Subscription_ Receive(ReceiverType_&& InReceiver)
	{
		ReceiversContainer.emplace_back(ReceiverState_::Active, NextReceiverIndex++, std::forward<ReceiverType_&&>(InReceiver));
		return { NextReceiverIndex - 1 };
	}

	static void Interrupt()
	{
		if (State != EventState_::Sending)
		{
			// Event is not sending, at moment of interruption;
			Assert(false);

			return;
		}

		State = EventState_::Interrupted;
	}

private:
	inline static EventState_ State = EventState_::Waiting;

	inline static ReceiverIndex_ NextReceiverIndex = 1;
	inline static std::vector<ReceiverData_> ReceiversContainer;
};

template <typename EventType_>
class Event_<EventType_>::ReceiverData_ final
{
public:
	ReceiverData_(ReceiverState_ InState, ReceiverIndex_ InIndex, const ReceiverFunction_& InFunction)
		: State(InState), Index(InIndex), Function(InFunction) {}

	ReceiverData_(ReceiverState_ InState, ReceiverIndex_ InIndex, ReceiverFunction_&& InFunction)
		: State(InState), Index(InIndex), Function(InFunction) {}

	ReceiverState_ State;
	ReceiverIndex_ Index;

	ReceiverFunction_ Function;
};

template <typename EventType_>
class Event_<EventType_>::Subscription_ final
{
	friend class Event_<EventType_>;

public:
	Subscription_() noexcept = default;

	[[nodiscard]] bool IsValid() const
	{
		return ReceiverIndex != 0;
	}

	void Pause() const
	{
		ForThis([](auto& InReceiver)
		{
			// Attempt to pause already paused or removed subscription;
			Assert(InReceiver.State == ReceiverState_::Active);

			InReceiver.State = ReceiverState_::Paused;
		});
	}

	void Resume() const
	{
		ForThis([](auto& InReceiver)
		{
			// Attempt to resume already active or removed subscription;
			Assert(InReceiver.State == ReceiverState_::Paused);

			InReceiver.State = ReceiverState_::Active;
		});
	}

	void Remove()
	{
		const auto& Begin = std::begin(ReceiversContainer);
		const auto& End = std::end(ReceiversContainer);

		const auto& Iterator = std::find_if(Begin, End, [this](const auto& InReceiver)
		{
			return InReceiver.Index == ReceiverIndex;
		});

		// Attempt to remove already removed subscription;
		Assert(Iterator != End);

		if (Iterator != End)
		{
			ReceiverIndex = 0;
			ReceiversContainer.erase(Iterator);
		}
	}

private:
	// Only for calling inside parent Event.
	Subscription_(ReceiverIndex_ InReceiverIndex) : ReceiverIndex(InReceiverIndex) {}

	void ForThis(std::function<void(ReceiverData_&)>&& InAction) const
	{
		for (auto& Receiver : ReceiversContainer)
		{
			if (Receiver.Index == ReceiverIndex)
			{
				InAction(Receiver);
				break;
			}
		}
	}

private:
	ReceiverIndex_ ReceiverIndex = 0;
};

//
// Example events and events users.
//

struct UpdateEvent_ final
{
	const float DeltaTime = 0.0f;
	const TrackingObject_ TrackingObject;
};

struct DrawEvent_ {};

class TestHandler_
{
public:
	void Handle(const UpdateEvent_& InEvent) const
	{
		std::cout << "Receive Event In Handler " << HandlerName << ":" << std::endl;
		std::cout << InEvent.DeltaTime << std::endl;
	}

	std::string HandlerName = "Test Handler";
};

class TestFunctionalObject_
{
public:
	void operator()(const UpdateEvent_& InEvent) const
	{
		std::cout << "Receive Event In Functional Object:" << std::endl;
		std::cout << InEvent.DeltaTime << std::endl;
	}
};

int main (int argc, char* argv[])
{
	TestHandler_ Handler;
	TestFunctionalObject_ FunctionalObject;

	Event_<UpdateEvent_>::Receive(FunctionalObject);
	Event_<UpdateEvent_>::Receive(std::bind(&TestHandler_::Handle, &Handler, std::placeholders::_1));
	Event_<UpdateEvent_>::Receive([](const auto& InEvent)
	{
		std::cout << "Receive Event In Lambda:" << std::endl;
		std::cout << InEvent.DeltaTime << std::endl;
	});

	std::cout << "\n-- Local event object:" << std::endl;
	UpdateEvent_ UpdateEvent {5.0f};
	Event_<UpdateEvent_>::Send(UpdateEvent);

	std::cout << "\n-- Temporary event object:" << std::endl;
	Event_<UpdateEvent_>::Send(UpdateEvent_{0.5f});

	std::cout << "\n-- Temporary event object list initialization:" << std::endl;
	Event_<UpdateEvent_>::Send({0.5f});

	std::cout << "\n-- Arguments forwarding:" << std::endl;
	Event_<UpdateEvent_>::Send(0.5f);

	std::cout << "\n-- Subscription management:" << std::endl;

	Event_<DrawEvent_>::Subscription_ Subscription;

	std::cout << "\nIs Valid: " << Subscription.IsValid() << std::endl;

	// Won't work because wrong subscription type.
	// Subscription = Event_<UpdateEvent_>::Receive({});

	Subscription = Event_<DrawEvent_>::Receive([](const auto&)
	{
		std::cout << "Receive Draw Event" << std::endl;
	});

	std::cout << "\nIs Valid: " << Subscription.IsValid() << std::endl;

	std::cout << "\n-- Test active state:" << Subscription.IsValid() << std::endl;
	Event_<DrawEvent_>::Send();

	Subscription.Pause();

	std::cout << "\n-- Test pause state:" << Subscription.IsValid() << std::endl;
	Event_<DrawEvent_>::Send();

	Subscription.Resume();

	std::cout << "\n-- Test resume:" << Subscription.IsValid() << std::endl;
	Event_<DrawEvent_>::Send();

	Subscription.Remove();

	std::cout << "\n-- Test remove:" << Subscription.IsValid() << std::endl;
	Event_<DrawEvent_>::Send();

	std::cout << "\nIs Valid: " << Subscription.IsValid() << std::endl;

	return 0;
}
