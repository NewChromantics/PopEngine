#include "TApiLeapMotion.h"

#include "Libs/LeapMotion/Leapv2/Leap.h"

namespace ApiLeapMotion
{
	const char Namespace[] = "Pop.LeapMotion";

	DEFINE_BIND_TYPENAME(Input);
	DEFINE_BIND_FUNCTIONNAME(GetNextFrame);
}


namespace Soy
{
	constexpr uint64_t	StringToEightcc(const char String[]);
	std::string			EightccToString(uint64_t Eightcc);
}


//	needs to be defined before use
constexpr uint64_t Soy::StringToEightcc(const char String[])
{
	uint64_t Value64 = 0;
	for ( auto i=0;	i<8;	i++ )
	{
		auto Char = String[i];
		if ( Char == 0 )
		break;
		auto Char64 = static_cast<uint64_t>(Char);
		auto Shift = i;
		Value64 |= Char64 << (8*Shift);
	}
	return Value64;
}


namespace LeapMotion
{
	class TFrame;
	class THand;
	class TJointPosition;
	
	//	named to be like an XR state
	namespace TState
	{
		enum TYPE
		{
			Disconnected,	//	maybe need to differentiate between errored & uninitialised state
			Connecting,
			Tracking,		//	in focus
			NotTracking,	//	not in focus
		};
	}
	
	
	//	get rid of string potential problems
	namespace TJoint
	{
		enum Type : uint64_t
		{
#define DEFINE_ENUM(Name)	Name = Soy::StringToEightcc( #Name )
			Invalid = Soy::StringToEightcc("null"),
			DEFINE_ENUM(Palm),
			DEFINE_ENUM(Wrist),
			DEFINE_ENUM(Elbow),
			DEFINE_ENUM(Thumb0),
			DEFINE_ENUM(Thumb1),
			DEFINE_ENUM(Thumb2),
			DEFINE_ENUM(Thumb3),
			DEFINE_ENUM(Index0),
			DEFINE_ENUM(Index1),
			DEFINE_ENUM(Index2),
			DEFINE_ENUM(Index3),
			DEFINE_ENUM(Middle0),
			DEFINE_ENUM(Middle1),
			DEFINE_ENUM(Middle2),
			DEFINE_ENUM(Middle3),
			DEFINE_ENUM(Ring0),
			DEFINE_ENUM(Ring1),
			DEFINE_ENUM(Ring2),
			DEFINE_ENUM(Ring3),
			DEFINE_ENUM(Pinky0),
			DEFINE_ENUM(Pinky1),
			DEFINE_ENUM(Pinky2),
			DEFINE_ENUM(Pinky3),
		};
	}
	
	const std::string		GetJointName(TJoint::Type Joint);
	TJoint::Type			GetJointName(const Leap::Finger& Finger,Leap::Bone::Type& BoneType);
	vec3f					GetPositionMetres(const Leap::Vector& Posmm);	//	leap motion coords are in mm, convert to metres
	void					GetDistortionPixels(SoyPixelsImpl& DistortionPixels,const Leap::Image& Image);
	void					GetInfraRedPixels(SoyPixelsImpl& InfraRedPixels,const Leap::Image& Image);
	SoyPixelsFormat::Type	GetPixelFormat(Leap::Image::FormatType Format,int BytesPerPixel);
}


std::string Soy::EightccToString(const uint64_t Eightcc)
{
	auto* Char = reinterpret_cast<const char*>( &Eightcc );
	return Char;
}

class LeapMotion::TJointPosition
{
public:
	TJoint::Type	mJoint = TJoint::Invalid;
	vec3f			mPosition;
};

class LeapMotion::THand
{
public:
	THand(){};
	THand(Leap::Hand& Hand);

	void		AddJoint(TJoint::Type Joint,const Leap::Vector& Positon);
	
	int32_t		mId = 0;
	std::string	mName;
	
	BufferArray<TJointPosition,40>	mJoints;
	
	vec3f		mPalmNormal;
	vec3f		mHandDirection;
	vec3f		mHandRotation;
	vec3f		mArmDirection;
};


class LeapMotion::TFrame
{
public:
	TFrame(){};
	TFrame(const Leap::Controller& Controller,Leap::Frame& Frame);
	
	int64_t							mFrameNumber = 0;
	SoyTime							mFrameTime;
	BufferArray<THand,10>			mHands;
	std::shared_ptr<SoyPixelsImpl>	mInfraRedPixels;
	std::shared_ptr<SoyPixelsImpl>	mInfraRedDistortion;
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
	void					SetState(TState::TYPE NewState);
	
	std::mutex				mLastFrameLock;
	TFrame					mLastFrame;
	std::atomic<bool>		mFrameChanged;
	std::function<void()>	mOnFrame;

	Leap::Controller		mController;
	TState::TYPE			mState = TState::Disconnected;
	std::function<void(TState::TYPE)>	mOnStateChanged;
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
	
	//	no data waiting
	if ( !mInput->HasNewFrame() )
		return;
	
	auto GetHandObject = [](Bind::TLocalContext& LocalContext,const LeapMotion::THand& Hand)
	{
		auto HandObject = LocalContext.mGlobalContext.CreateObjectInstance(LocalContext);

		HandObject.SetInt("HandId", Hand.mId );
		for ( auto j=0;	j<Hand.mJoints.GetSize();	j++ )
		{
			auto& Joint = Hand.mJoints[j];
			auto xyz = GetRemoteArray( &Joint.mPosition.x, 3 );
			auto JointName = LeapMotion::GetJointName(Joint.mJoint);
			HandObject.SetArray( JointName, GetArrayBridge(xyz) );
		}
		return HandObject;
	};
	
	auto GetFrameObject = [&](Bind::TLocalContext& LocalContext,Bind::TObject& FrameObject,const LeapMotion::TFrame& Frame)
	{
		FrameObject.SetInt("FrameNumber",(int32_t)Frame.mFrameNumber);
		//FrameObject.SetInt("FrameTime",Frame.mFrameTime);//	too big for js!
		
		for ( auto h=0;	h<Frame.mHands.GetSize();	h++ )
		{
			auto& Hand = Frame.mHands[h];
			auto HandObject = GetHandObject( LocalContext, Hand );
			FrameObject.SetObject( Hand.mName, HandObject );
		}
	};
	
	//	grab all the buffered frames?
	try
	{
		auto Frame = mInput->PopFrame();
		
		auto ReturnFrame = [&](Bind::TLocalContext& LocalContext,Bind::TPromise& Promise)
		{
			auto FrameObject = LocalContext.mGlobalContext.CreateObjectInstance(LocalContext);
			GetFrameObject( LocalContext, FrameObject, Frame );
			
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

void LeapMotion::TInput::SetState(TState::TYPE NewState)
{
	mState = NewState;
	if ( mOnStateChanged )
		mOnStateChanged( mState );
}


LeapMotion::TFrame LeapMotion::TInput::PopFrame()
{
	if ( !mFrameChanged )
		throw Soy::AssertException("No new frame");
	
	std::lock_guard<std::mutex> Lock( mLastFrameLock );
	mFrameChanged = false;
	return mLastFrame;
}



const std::string LeapMotion::GetJointName(TJoint::Type Joint)
{
	return Soy::EightccToString( Joint );
}

LeapMotion::TJoint::Type LeapMotion::GetJointName(const Leap::Finger& Finger,Leap::Bone::Type& BoneType)
{
	constexpr auto FingerCount = 5;
	constexpr auto BoneCount = 4;
	constexpr TJoint::Type JointBoneNames[FingerCount][BoneCount] =
	{
		{	TJoint::Thumb0, TJoint::Thumb1, TJoint::Thumb2, TJoint::Thumb3	},
		{	TJoint::Index0, TJoint::Index1, TJoint::Index2, TJoint::Index3	},
		{	TJoint::Middle0, TJoint::Middle1, TJoint::Middle2, TJoint::Middle3	},
		{	TJoint::Ring0, TJoint::Ring1, TJoint::Ring2, TJoint::Ring3	},
		{	TJoint::Pinky0, TJoint::Pinky1, TJoint::Pinky2, TJoint::Pinky3	},
	};
	
	auto FingerIndex = static_cast<int>(Finger.type());
	auto BoneIndex = static_cast<int>(BoneType);

	if ( FingerIndex < 0 || FingerIndex >= FingerCount )
		throw Soy::AssertException("Finger index out of range");
	if ( BoneIndex < 0 || BoneIndex >= BoneCount )
		throw Soy::AssertException("Bone index out of range");
	
	return JointBoneNames[FingerIndex][BoneType];
}


SoyPixelsFormat::Type LeapMotion::GetPixelFormat(Leap::Image::FormatType Format,int BytesPerPixel)
{
	if ( Format != Leap::Image::INFRARED )
		throw Soy::AssertException("LeapMotion unknown image type (not infrared)");
	if ( BytesPerPixel != 1 )
		throw Soy::AssertException("LeapMotion image BytesPerPixel not 1");

	return SoyPixelsFormat::Greyscale;
}


void LeapMotion::GetInfraRedPixels(SoyPixelsImpl& InfraRedPixels,const Leap::Image& Image)
{
	auto PixelFormat = GetPixelFormat( Image.format(), Image.bytesPerPixel() );
	SoyPixelsMeta Meta( Image.width(), Image.height(), PixelFormat );
	auto* PixelBuffer = const_cast<uint8_t*>(Image.data());
	auto PixelBufferSize = Image.width() * Image.height() * Image.bytesPerPixel();
	SoyPixelsRemote Pixels( PixelBuffer, PixelBufferSize, Meta );
	InfraRedPixels.Copy(Pixels);
}


void LeapMotion::GetDistortionPixels(SoyPixelsImpl& DistortionPixels,const Leap::Image& Image)
{
	auto PixelFormat = SoyPixelsFormat::Float1;
	SoyPixelsMeta Meta( Image.distortionWidth(), Image.distortionHeight(), PixelFormat );
	auto* PixelBuffer = reinterpret_cast<uint8_t*>( const_cast<float*>( Image.distortion() ) );
	auto PixelBufferSize = Meta.GetDataSize();
	SoyPixelsRemote Pixels( PixelBuffer, PixelBufferSize, Meta );
	DistortionPixels.Copy(Pixels);
}


LeapMotion::TFrame::TFrame(const Leap::Controller& Controller,Leap::Frame& Frame) :
	mFrameNumber	( Frame.id() )
{
	if ( !Frame.isValid() )
		throw Soy::AssertException("Trying to construct an invalid Leap::Frame");
	
	//	time is in microsecs, convert to ms
	std::chrono::microseconds FrameTimeMicros( Frame.timestamp() );
	std::chrono::microseconds LeapTimeMicros( Controller.now() );

	auto AgeMicros = (LeapTimeMicros - FrameTimeMicros);
	/*
	std::chrono::milliseconds AgeMs;
	AgeMs = (AgeMicros);
	if ( AgeMs < 0 )
	{
		std::Debug << "Not expecting leap frame to be in the future: " << AgeMs << "ms" << std::endl;
		AgeMs = 0;
	}
	 */
	auto AgeMs = 0;
	SoyTime Now(true);
	Now.mTime -= AgeMs;
	mFrameTime = Now;
	
	//	get hands
	auto Hands = Frame.hands();
	for ( auto it=Hands.begin();	it!=Hands.end();	it++ )
	{
		auto LeapHand = *it;
		LeapMotion::THand Hand( LeapHand );
		mHands.PushBack( Hand );
	}
	
	auto Images = Frame.images();
	for ( auto it=Images.begin();	it!=Images.end();	it++ )
	{
		auto& Image = *it;
		try
		{
			std::shared_ptr<SoyPixelsImpl> InfraRedPixels( new SoyPixels );
			GetInfraRedPixels( *InfraRedPixels, Image );
			mInfraRedPixels = InfraRedPixels;
			
			std::shared_ptr<SoyPixelsImpl> DistortionPixels( new SoyPixels );
			GetDistortionPixels( *DistortionPixels, Image );
			mInfraRedDistortion = DistortionPixels;
		}
		catch (std::exception& e)
		{
			std::Debug << "Error getting frame image: " << e.what() << std::endl;
		}
	}
}


LeapMotion::THand::THand(Leap::Hand& Hand) :
	mId		( Hand.id() ),
	mName	( Hand.isLeft() ? "Left":"Right" )
{
	AddJoint( TJoint::Palm, Hand.palmPosition() );
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
	AddJoint( TJoint::Wrist, Arm.wristPosition() );
	AddJoint( TJoint::Elbow, Arm.elbowPosition() );
	
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


void LeapMotion::THand::AddJoint(TJoint::Type Joint,const Leap::Vector& Positon)
{
	auto& NewJoint = mJoints.PushBack();
	NewJoint.mJoint = Joint;
	NewJoint.mPosition = GetPositionMetres( Positon );
}

vec3f LeapMotion::GetPositionMetres(const Leap::Vector& Posmm)
{
	vec3f Pos( Posmm.x, Posmm.y, Posmm.z );
	//	mm to metres
	Pos.x /= 1000.f;
	Pos.y /= 1000.f;
	Pos.z /= 1000.f;
	
	//	z is opposite to what we use too
	//Pos.z = -Pos.z;
	
	return Pos;
}
	
void LeapMotion::TInput::onInit(const Leap::Controller&)
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
	SetState( TState::Connecting );
}

void LeapMotion::TInput::onConnect(const Leap::Controller&)
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
	SetState( TState::NotTracking );
}

void LeapMotion::TInput::onDisconnect(const Leap::Controller&)
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
	SetState( TState::Disconnected );
}

void LeapMotion::TInput::onExit(const Leap::Controller&)
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
	SetState( TState::Disconnected );
}

void LeapMotion::TInput::onFrame(const Leap::Controller& Controller)
{
	//std::Debug << __PRETTY_FUNCTION__ << std::endl;
	auto LeapFrame = Controller.frame();
	TFrame Frame( Controller, LeapFrame );
	
	//	if no hands, skip
	if ( Frame.mHands.IsEmpty() )
		return;
	
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
	SetState( TState::Tracking );
}

void LeapMotion::TInput::onFocusLost(const Leap::Controller&)
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
	SetState( TState::NotTracking );
}

void LeapMotion::TInput::onDeviceChange(const Leap::Controller&)
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
}

void LeapMotion::TInput::onServiceConnect(const Leap::Controller&)
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
	//	how is this different from onconnect/disconnect?
}

void LeapMotion::TInput::onServiceDisconnect(const Leap::Controller&)
{
	std::Debug << __PRETTY_FUNCTION__ << std::endl;
}

