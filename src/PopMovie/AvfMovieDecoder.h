#pragma once

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <map>

#include <SoyEvent.h>
#include <SoyThread.h>

#if defined(__OBJC__)
#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#endif

#include <SoyMedia.h>
#include "AvfVideoCapture.h"

class AvfMovieDecoder;
class AvfDecoderRenderer;
class AvfTextureCache;

namespace Platform
{
	std::shared_ptr<TMediaExtractor>	AllocVideoDecoder(TMediaExtractorParams Params,std::shared_ptr<Opengl::TContext> OpenglContext);
}


namespace TLoadAssetTracksState
{
	enum Type
	{
		NotStarted,
		Loading,
		Ready,
		Failed,
	};
}




#if defined(__OBJC__)
class AvfDecoder : public AvfMediaExtractor
{
public:
	AvfDecoder(const TMediaExtractorParams& Params,std::shared_ptr<Opengl::TContext>& OpenglContext);
	~AvfDecoder()
	{
		Shutdown();
	}
	
	bool		IsPlaying()		{	return true;	}	//	push blocking, maybe need a play/pause/stopped state?

protected:
	void		Shutdown();
	
protected:
	float3x3	mTransform;
	std::shared_ptr<Opengl::TContext>		mOpenglContext;	//	need to store context we're using for the shutdown
	std::shared_ptr<AvfDecoderRenderer>		mRenderer;	//	persistent rendering data
};
#endif


#if defined(__OBJC__)
class AvfAsset
{
public:
	AvfAsset(const std::string& Filename,std::function<bool(const TStreamMeta&)> FilterTrack=nullptr);
	
	void				LoadTracks(const std::function<bool(const TStreamMeta&)>& FilterTrack);	//	blocking & throwing load of desired track[s]
	AVAssetTrack*		GetTrack(size_t TrackIndex);

	std::shared_ptr<Platform::TMediaFormat>		GetStreamFormat(size_t StreamIndex)	{	return mStreamFormats[StreamIndex];	}

public:
	Array<TStreamMeta>	mStreams;
	ObjcPtr<AVAsset>	mAsset;
	std::map<size_t,std::shared_ptr<Platform::TMediaFormat>>	mStreamFormats;
};
#endif

//	start merging these
#if defined(__OBJC__)
class AvfAssetDecoder : public AvfDecoder
{
public:
	AvfAssetDecoder(const TMediaExtractorParams& Params,std::shared_ptr<Opengl::TContext>& OpenglContext) :
		AvfDecoder		( Params, OpenglContext )
	//	SoyWorkerThread	( std::string("AvfAssetDecoder/")+Params.mFilename, SoyWorkerWaitMode::NoWait )
	{
		
	}

protected:
	virtual bool			Iteration() override;
	virtual void			CreateReader()=0;
	virtual void			DeleteReader()=0;
	virtual bool			WaitForReaderNextFrame()=0;

public:
	std::string				mCaughtDecodingException;		//	any exceptions caught during iteration
	float3x3				mAffineTransform;
};
#endif



#if defined(__OBJC__)
class AvfMovieDecoder : public AvfAssetDecoder
{
public:
	AvfMovieDecoder(const TMediaExtractorParams& Params,std::shared_ptr<Opengl::TContext>& OpenglContext);
	~AvfMovieDecoder();
	
	bool		HasError();			//	check this to see if it's failed
	
	void		CreateAsset();
	
	//	because creating the video track can fail at startReading, we need to delete the whole AvAssetReader and start again, we wrap the whole thing
	virtual void	CreateReader() override;
	bool			CreateReader(id PixelFormat,bool OpenglCompatibility);
	void			CreateReaderObject();
	void			CreateVideoTrack(id PixelFormat,bool OpenglCompatibility);
	virtual void	DeleteReader() override;
	void			DeleteAsset();
	
	virtual bool	WaitForReaderNextFrame() override;
	
	bool				CopyBuffer(CMSampleBufferRef Buffer,std::stringstream& Error);
	
public:
	std::shared_ptr<AvfAsset>	mAsset;
	bool						mAvfCopyBuffer;
	bool						mCopyBuffer;
	
	ObjcPtr<AVAssetReader>	mReader;
	ObjcPtr<AVAssetReaderTrackOutput>	mVideoTrackOutput;
	std::mutex				mVideoTrackOutputLock;		//	lock during the blocking CopySample so we don't cancel reading until this has passed and vice versa
};
#endif


#if defined(__OBJC__)
@class AvfPlayerDelegate;
#endif


//	movie decoder using AvPlayer instead of AVAssetReader
#if defined(__OBJC__)
class AvfDecoderPlayer : public AvfAssetDecoder
{
public:
	AvfDecoderPlayer(const TMediaExtractorParams& Params,std::shared_ptr<Opengl::TContext>& OpenglContext);
	~AvfDecoderPlayer();
	
	void					ReAddOutput();
	
protected:
	virtual void			CreateReader() override;
	virtual void			DeleteReader() override;
	virtual bool			WaitForReaderNextFrame() override;
	SoyTime					GetCurrentTime();
	
	void					OnSeekCompleted(SoyTime RequestedSeekTime,bool Finished);

	void					WaitForLoad();	//	blocks until loading finished. throws on error
	
	
public:
	//	meta... per stream though?
	float3x3				mAffineTransform;
	SoyTime					mDuration;
	
	ObjcPtr<AVPlayerItem>				mPlayerItem;
	
	ObjcPtr<AVPlayer>					mPlayer;
	ObjcPtr<AVPlayerItemVideoOutput>	mPlayerVideoOutput;
	ObjcPtr<AvfPlayerDelegate>			mPlayerDelegate;
};
#endif


#if defined(__OBJC__)
@interface AvfPlayerDelegate : NSObject <AVPlayerItemOutputPullDelegate>
{
	AvfDecoderPlayer*	mParent;
}

- (id)initWithParent:(AvfDecoderPlayer*)parent;
- (void)outputMediaDataWillChange:(AVPlayerItemOutput *)sender;
- (void)outputSequenceWasFlushed:(AVPlayerItemOutput *)output;

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context;


@end
#endif

