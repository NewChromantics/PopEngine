#include "TApiLeapMotion.h"

#include "Libs/LeapMotion/Leapv2/Leap.h"

namespace ApiLeapMotion
{
	const char Namespace[] = "Pop.LeapMotion";

	DEFINE_BIND_TYPENAME(Input);
	DEFINE_BIND_FUNCTIONNAME(GetNextFrame);
}

namespace LeapMotion
{
	class TFrame;
	class THand;
	class TJoint;
}


class LeapMotion::TJoint
{
public:
	const char*	mName = nullptr;
	vec3f		mPosition;
};

class LeapMotion::THand
{
public:
	THand(){};
	THand(Leap::Hand& Hand);

	void		AddJoint(const char* Name,const Leap::Vector& Positon);
	
	int32_t		mId = 0;
	std::string	mName;
	
	BufferArray<TJoint,40>	mJoints;
	
	vec3f		mPalmNormal;
	vec3f		mHandDirection;
	vec3f		mHandRotation;
	vec3f		mArmDirection;
};


class LeapMotion::TFrame
{
public:
	TFrame(){};
	TFrame(Leap::Frame& Frame);
	
	int64_t	mFrameNumber = 0;
	int64_t	mFrameTime = 0;	// Use Controller::now() to calculate the age of the frame.
	BufferArray<THand,10>	mHands;
};

class LeapMotion::TInput : public Leap::Listener
{
public:
	TInput();
	~TInput();
	
	
	virtual void onInit(const Leap::Controller&) override;
	virtual void onConnect(const Leap::Controller&) override;
	virtual void onDisconnect(const Leap::Controller&) override;
	virtual void onExit(const Leap::Controller&) override;
	virtual void onFrame(const Leap::Controller&) override;
	virtual void onFocusGained(const Leap::Controller&) override;
	virtual void onFocusLost(const Leap::Controller&) override;
	virtual void onDeviceChange(const Leap::Controller&) override;
	virtual void onServiceConnect(const Leap::Controller&) override;
	virtual void onServiceDisconnect(const Leap::Controller&) override;
	
	bool					HasNewFrame() const	{	return mFrameChanged;	}
	TFrame					PopFrame();
	
	Leap::Controller		mController;
	std::function<void()>	mOnFrame;
	std::mutex				mLastFrameLock;
	TFrame					mLastFrame;
	std::atomic<bool>		mFrameChanged;
};


void ApiLeapMotion::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);

	Context.BindObjectType<TInputWrapper>( Namespace );
}

void ApiLeapMotion::TInputWrapper::Construct(Bind::TCallback& Params)
{
	mInput.reset( new LeapMotion::TInput() );
	
	mInput->mOnFrame = [this]()
	{
		OnFramesChanged();
	};
}


void ApiLeapMotion::TInputWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<ApiLeapMotion::BindFunction::GetNextFrame>( &TInputWrapper::GetNextFrame );
}



void ApiLeapMotion::TInputWrapper::GetNextFrame(Bind::TCallback& Params)
{
	auto Promise = mOnFramePromises.AddPromise( Params.mLocalContext );
	
	//	flush any frames
	OnFramesChanged();
	
	Params.Return( Promise );
}

void ApiLeapMotion::TInputWrapper::OnFramesChanged()
{
	if ( !mInput )
	{
		mOnFramePromises.Reject("Input null");
		return;
	}
	
	//	nothing waiting
	if ( !mOnFramePromises.HasPromises() )
		return;
	
	//	grab all the buffered frames?
	try
	{
		auto Frame = mInput->PopFrame();
		
		auto ReturnFrame = [&](Bind::TLocalContext& LocalContext,Bind::TPromise& Promise)
		{
			auto FrameObject = LocalContext.mGlobalContext.CreateObjectInstance(LocalContext);
			FrameObject.SetInt("FrameNumber",Frame.mFrameNumber);
			//FrameObject.SetInt("FrameTime",Frame.mFrameTime);//	too big for js!

			for ( auto h=0;	Frame.mHands.GetSize();	h++ )
			{
				auto& Hand = Frame.mHands[h];
				auto HandObject = LocalContext.mGlobalContext.CreateObjectInstance(LocalContext);
				
				HandObject.SetInt("HandId", Hand.mId );
				for ( auto j=0;	j<Hand.mJoints.GetSize();	j++ )
				{
					auto& Joint = Hand.mJoints[j];
					auto xyz = GetRemoteArray( &Joint.mPosition.x, 3 );
					HandObject.SetArray(Joint.mName, GetArrayBridge(xyz) );
				}
				
				FrameObject.SetObject( Hand.mName, HandObject );
			}
			Promise.Resolve( LocalContext, FrameObject );
		};
		mOnFramePromises.Flush(ReturnFrame);
	}
	catch(std::exception& e)
	{
		mOnFramePromises.Reject( e.what() );
	}

}



LeapMotion::TInput::TInput() :
	mFrameChanged	( false )
{
	mController.addListener( *this );
	//mController.setPolicy(Leap::Controller::POLICY_BACKGROUND_FRAMES);
}

LeapMotion::TInput::~TInput()
{
	mController.removeListener( *this );
}


LeapMotion::TFrame LeapMotion::TInput::PopFrame()
{
	if ( !mFrameChanged )
		throw Soy::AssertException("No new frame");
	
	std::lock_guard<std::mutex> Lock( mLastFrameLock );
	mFrameChanged = false;
	return mLastFrame;
}


const char* GetJointName(const Leap::Finger& Finger,Leap::Bone::Type& BoneType)
{
	const char* JointBoneNames[5][4] =
	{
		{	"Thumb0", "Thumb1", "Thumb2", "Thumb3"	},
		{	"IndexFinger0", "IndexFinger1", "IndexFinger2", "IndexFinger3"	},
		{	"MiddleFinger0", "MiddleFinger1", "MiddleFinger2", "MiddleFinger3"	},
		{	"RingFinger0", "RingFinger1", "RingFinger2", "RingFinger3"	},
		{	"PinkyFinger0", "PinkyFinger1", "PinkyFinger2", "PinkyFinger3"	},
	};
	return JointBoneNames[Finger.type()][BoneType];
}

LeapMotion::THand::THand(Leap::Hand& Hand) :
	mId		( Hand.id() ),
	mName	( Hand.isLeft() ? "Left":"Right" )
{
	AddJoint("Palm", Hand.palmPosition() );
	//mPalmNormal = Hand.palmNormal();
	//mDirection = Hand.direction();
/*
	// Calculate the hand's pitch, roll, and yaw angles
	std::cout << std::string(2, ' ') <<  "pitch: " << direction.pitch() * RAD_TO_DEG << " degrees, "
	<< "roll: " << normal.roll() * RAD_TO_DEG << " degrees, "
	<< "yaw: " << direction.yaw() * RAD_TO_DEG << " degrees" << std::endl;
	*/
	auto Arm = Hand.arm();
	//mArmDirection = Arm.direction();
	AddJoint("Wrist", Arm.wristPosition() );
	AddJoint("Elbow", Arm.elbowPosition() );
	
	// Get fingers
	auto FingerList = Hand.fingers();
	for ( auto it=FingerList.begin();	it!=FingerList.end();	it++ )
	{
		auto& Finger = *it;
		
		auto EnumBone = [&](Leap::Bone::Type Type)
		{
			auto Bone = Finger.bone(Type);
			//	gr: return null to javascript here?
			if ( !Bone.isValid() )
				return;
			
			//	use the "end" of the joint? will that skip the knuckle?
			AddJoint( GetJointName(Finger,Type), Bone.nextJoint() );
		};
		EnumBone( Leap::Bone::TYPE_METACARPAL );
		EnumBone( Leap::Bone::TYPE_PROXIMAL );
		EnumBone( Leap::Bone::TYPE_INTERMEDIATE );
		EnumBone( Leap::Bone::TYPE_DISTAL );
	}
		
}


void LeapMotion::THand::AddJoint(const char* Name,const Leap::Vector& Positon)
{
	auto 
	mJoints.
}
	
	
void LeapMotion::TInput::onInit(const Leap::Controller&)
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
}

void LeapMotion::TInput::onConnect(const Leap::Controller&)
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
}

void LeapMotion::TInput::onDisconnect(const Leap::Controller&)
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
}

void LeapMotion::TInput::onExit(const Leap::Controller&)
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
}

void LeapMotion::TInput::onFrame(const Leap::Controller& Controller)
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
	auto LeapFrame = Controller.frame();
	TFrame Frame( LeapFrame );
	
	{
		std::lock_guard<std::mutex> Lock(mLastFrameLock);
		mLastFrame = Frame;
		mFrameChanged = true;
	}
	
	//	callback
	if ( mOnFrame )
		mOnFrame();
}

void LeapMotion::TInput::onFocusGained(const Leap::Controller&)
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
}

void LeapMotion::TInput::onFocusLost(const Leap::Controller&)
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
}

void LeapMotion::TInput::onDeviceChange(const Leap::Controller&)
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
}

void LeapMotion::TInput::onServiceConnect(const Leap::Controller&)
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
}

void LeapMotion::TInput::onServiceDisconnect(const Leap::Controller&)
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
}

