using UnityEngine;
using System.Collections;					// required for Coroutines
using System.Runtime.InteropServices;		// required for DllImport
using System;								// requred for IntPtr



public class TTexturePtrCache<TEXTURE> where TEXTURE : Texture
{
	public TEXTURE			mTexture;
	public System.IntPtr	mPtr;
};

public class TexturePtrCache
{
	static public System.IntPtr GetCache<T>(ref TTexturePtrCache<T> Cache,T texture) where T : Texture
	{
		if (Cache==null)
			return texture.GetNativeTexturePtr();

		if ( texture.Equals(Cache.mTexture) )
			return Cache.mPtr;
		Cache.mPtr = texture.GetNativeTexturePtr();
		if ( Cache.mPtr != System.IntPtr.Zero )
			Cache.mTexture = texture;
		return Cache.mPtr;	
	}
	
	static public System.IntPtr GetCache(ref TTexturePtrCache<RenderTexture> Cache,RenderTexture texture)
	{
		if (Cache==null)
			return texture.GetNativeTexturePtr();

		if ( texture.Equals(Cache.mTexture) )
			return Cache.mPtr;
		var Prev = RenderTexture.active;
		RenderTexture.active = texture;
		Cache.mPtr = texture.GetNativeTexturePtr();
		RenderTexture.active = Prev;
		if ( Cache.mPtr != System.IntPtr.Zero )
			Cache.mTexture = texture;
		return Cache.mPtr;	
	}
};


public class  PopMovieParams
{
	public ulong			mPreSeekMs = 0;						//	start decoding at a particular point in video. Can be used to pre-buffer to aid synchronisation
	public bool				mSkipPushFrames = false;			//	skip queuing old frames after decoding if in the past
	public bool				mSkipPopFrames = true;				//	skip old frames when possible when presenting latest (synchronisation)
	public bool				mAllowGpuColourConversion = true;	//	allow colour conversion on graphics driver - often done in CPU (depends on platform/driver)
	public bool				mAllowCpuColourConversion = false;	//	allow internal colour conversion on CPU. Slow!
	public bool				mAllowSlowCopy = true;				//	allow CPU->GPU pixel copying
	public bool				mAllowFastCopy = true;				//	allow GPU->GPU copying where availible
	public bool				mPixelClientStorage = false;		//	on OSX allow storing pixels on host/cpu rather than uploading to GPU (faster CPU copy)
	public bool				mDebugFrameSkipping = true;			//	print out messages when we drop/skip frames for various reasons
	public bool				mPeekBeforeDefferedCopy = true;		//	don't queue a copy (for when we do copies on the graphics thread) if RIGHT NOW there is no new frame. Can reduce wasted time on graphics thread
	public bool				mDecodeAsYuvFullRange = false;		//	decode as YUV (full range in luma channel)
	public bool				mDecodeAsYuvVideo = false;			//	decode as YUV (SDTV range in luma channel)
	public bool				mDebugNoNewPixelBuffer = false;		//	print out when there is no frame to pop. Useful if frames aren't appearing with no error
	public bool				mDebugRenderThreadCallback = false;	//	turn on to show that unity is calling the plugin's graphics thread callback (essential for multithreaded rendering, and often is a problem with staticcly linked plugins - like ios)
	public bool				mResetInternalTimestamp = true;		//	if your source video's timestamps don't start at 0, this resets them so the first frame becomes 0
	public bool				mDebugBlit = false;					//	use the test shader for blitting
	public bool				mApplyVideoTransform = true;		//	apply the transform found inside the video
	public uint				mVideoTrackIndex = 0;				//	in case your video has multiple tracks, and you don't want 0
	public bool				mGenerateMipMaps = true;			//	generate mip maps onto textures. Mostly for dev debugging, but some platforms don't require this and can be an optimisation when targetting limited devices/platforms
}


public class PopMovie
{
#if UNITY_STANDALONE_OSX || UNITY_EDITOR_OSX
	private const string PluginName = "PopMovieTextureOsx";
#elif UNITY_ANDROID
	private const string PluginName = "PopMovieTexture";
#elif UNITY_IOS
	//private const string PluginName = "PopMovieTextureIos";
	private const string PluginName = "__Internal";
#elif UNITY_STANDALONE_WIN || UNITY_EDITOR_WIN
	private const string PluginName = "PopMovieTexture";
#endif

	//	gr: after a huge amount of headaches... we're passing a bitfield for params. NOT a struct
	private enum PopMovieFlags
	{
		None						= 0,
		SkipPushFrames				= 1<<0,
		SkipPopFrames				= 1<<1,
		AllowGpuColourConversion	= 1<<2,
		AllowCpuColourConversion	= 1<<3,
		AllowSlowCopy				= 1<<4,
		AllowFastCopy				= 1<<5,
		PixelClientStorage			= 1<<6,
		DebugFrameSkipping			= 1<<7,
		PeekBeforeDefferedCopy		= 1<<8,
		DecodeAsYuvFullRange		= 1<<9,
		DecodeAsYuvVideo			= 1<<10,
		DebugNoNewPixelBuffer		= 1<<11,
		DebugRenderThreadCallback	= 1<<12,
		ResetInternalTimestamp		= 1<<13,
		DebugBlit					= 1<<14,
		ApplyVideoTransform			= 1<<15,
		GenerateMipMaps				= 1<<16,
	};

    private ulong							mInstance = 0;
	private static int						mPluginEventId = PopMovie_GetPluginEventId();
	private float							mStartTime = -1;	//	auto play (auto-set time on UpdateTexture) if this is not <0

	//	cache the texture ptr's. Unity docs say accessing them causes a GPU sync, I don't believe they do, BUT we want to avoid setting the active render texture anyway
	private TTexturePtrCache<Texture2D>		mTexture2DPtrCache = new TTexturePtrCache<Texture2D>();
	private TTexturePtrCache<RenderTexture>	mRenderTexturePtrCache = new TTexturePtrCache<RenderTexture>();

	[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
	public delegate void DebugLogDelegate(string str);
	private DebugLogDelegate	mDebugLogDelegate = null;
	
	public delegate void OnFinishedDelegate();
	private OnFinishedDelegate	mOnFinishedDelegate = null;
	
	public void AddDebugCallback(DebugLogDelegate Function)
	{
		if ( mDebugLogDelegate == null ) {
			mDebugLogDelegate = new DebugLogDelegate (Function);
		} else {
			mDebugLogDelegate += Function;
		}
	}
	
	public void RemoveDebugCallback(DebugLogDelegate Function)
	{
		if ( mDebugLogDelegate != null ) {
			mDebugLogDelegate -= Function;
		}
	}

	public void AddOnFinishedCallback(OnFinishedDelegate Function)
	{
		if ( mOnFinishedDelegate == null ) {
			mOnFinishedDelegate = new OnFinishedDelegate (Function);
		} else {
			mOnFinishedDelegate += Function;
		}
	}
	
	public void RemoveOnFinishedCallback(OnFinishedDelegate Function)
	{
		if ( mOnFinishedDelegate != null ) {
			mOnFinishedDelegate -= Function;
		}
	}

	void DebugLog(string Message)
	{
		if ( mDebugLogDelegate != null )
			mDebugLogDelegate (Message);
	}

	public bool IsAllocated()
	{
		return mInstance != 0;
	}
	
	[DllImport (PluginName, CallingConvention=CallingConvention.Cdecl)]
	private static extern ulong		PopMovie_Alloc(string Filename,uint Params,ulong PreSeekMs,uint VideoTrackIndex);

	[DllImport (PluginName)]
	private static extern bool		PopMovie_Free(ulong Instance);
	
	[DllImport (PluginName)]
	private static extern bool		PopMovie_UpdateTexture2D(ulong Instance,System.IntPtr TextureId,int Width,int Height,TextureFormat textureFormat);

	[DllImport (PluginName)]
	private static extern bool		PopMovie_UpdateRenderTexture(ulong Instance,System.IntPtr TextureId,int Width,int Height,RenderTextureFormat textureFormat);

	[DllImport (PluginName)]
	private static extern bool		PopMovie_SetTime(ulong Instance,ulong TimeMs);
	
	[DllImport (PluginName)]
	private static extern ulong		PopMovie_GetDurationMs(ulong Instance);
	
	[DllImport (PluginName)]
	private static extern int		PopMovie_GetPluginEventId();
	
	[DllImport (PluginName)]
	private static extern bool		FlushDebug([MarshalAs(UnmanagedType.FunctionPtr)]System.IntPtr FunctionPtr);

	[DllImport (PluginName)]
	private static extern void		QueueOpenglJob([MarshalAs(UnmanagedType.FunctionPtr)]System.IntPtr FunctionPtr);

	[DllImport (PluginName)]
	private static extern void		QueueTextureBlit(System.IntPtr SourceTexture,System.IntPtr DestinationTexture,int DestinationWidth,int DestinationHeight);


	public PopMovie(string Filename,PopMovieParams Params,bool AutoPlay)
	{
#if UNITY_EDITOR && UNITY_IOS
		UnityEditor.ScriptingImplementation backend = (UnityEditor.ScriptingImplementation)UnityEditor.PlayerSettings.GetPropertyInt("ScriptingBackend", UnityEditor.BuildTargetGroup.iOS);
		if (backend != UnityEditor.ScriptingImplementation.IL2CPP) {
			Debug.LogWarning ("Warning: If the scripting backend is not IL2CPP on IOS there may be problems at runtime passing parameters to the plugin.");
		}
#endif

		PopMovieFlags ParamFlags = 0;
		ParamFlags |= Params.mAllowCpuColourConversion	? PopMovieFlags.AllowCpuColourConversion : PopMovieFlags.None;
		ParamFlags |= Params.mAllowFastCopy				? PopMovieFlags.AllowFastCopy : PopMovieFlags.None;
		ParamFlags |= Params.mAllowGpuColourConversion	? PopMovieFlags.AllowGpuColourConversion : PopMovieFlags.None;
		ParamFlags |= Params.mAllowSlowCopy				? PopMovieFlags.AllowSlowCopy : PopMovieFlags.None;
		ParamFlags |= Params.mApplyVideoTransform		? PopMovieFlags.ApplyVideoTransform : PopMovieFlags.None;
		ParamFlags |= Params.mDebugBlit					? PopMovieFlags.DebugBlit : PopMovieFlags.None;
		ParamFlags |= Params.mDebugFrameSkipping		? PopMovieFlags.DebugFrameSkipping : PopMovieFlags.None;
		ParamFlags |= Params.mDebugNoNewPixelBuffer 	? PopMovieFlags.DebugNoNewPixelBuffer : PopMovieFlags.None;
		ParamFlags |= Params.mDebugRenderThreadCallback	? PopMovieFlags.DebugRenderThreadCallback : PopMovieFlags.None;
		ParamFlags |= Params.mDecodeAsYuvFullRange		? PopMovieFlags.DecodeAsYuvFullRange : PopMovieFlags.None;
		ParamFlags |= Params.mDecodeAsYuvVideo			? PopMovieFlags.DecodeAsYuvVideo : PopMovieFlags.None;
		ParamFlags |= Params.mPeekBeforeDefferedCopy	? PopMovieFlags.PeekBeforeDefferedCopy : PopMovieFlags.None;
		ParamFlags |= Params.mPixelClientStorage		? PopMovieFlags.PixelClientStorage : PopMovieFlags.None;
		ParamFlags |= Params.mResetInternalTimestamp	? PopMovieFlags.ResetInternalTimestamp : PopMovieFlags.None;
		ParamFlags |= Params.mSkipPopFrames				? PopMovieFlags.SkipPopFrames : PopMovieFlags.None;
		ParamFlags |= Params.mSkipPushFrames 			? PopMovieFlags.SkipPushFrames : PopMovieFlags.None;
		ParamFlags |= Params.mGenerateMipMaps 			? PopMovieFlags.GenerateMipMaps : PopMovieFlags.None;

		uint ParamFlags32 = Convert.ToUInt32 (ParamFlags);
		string FinalFilename = GetVideoFilename (Filename);
		mInstance = PopMovie_Alloc ( FinalFilename, ParamFlags32, Params.mPreSeekMs, Params.mVideoTrackIndex );

		//	if this fails, capture the flush and throw an exception
		if (mInstance == 0) {
			string AllocError = "";
			FlushDebug (
				(string Error) => {
				AllocError += Error; }
			);
			if ( AllocError.Length == 0 )
				AllocError = "No error detected";
			throw new System.Exception("Failed to allocate PopMovieTexture: " + AllocError);
		}

		if (AutoPlay)
			mStartTime = Time.time;
	}
	
	~PopMovie()
	{
		//	gr: don't quite get the destruction order here, but need to remove the [external] delegates in destructor. 
		//	Assuming external delegate has been deleted, and this garbage collection (despite being explicitly called) 
		//	is still deffered until after parent object[monobehaviour] has been destroyed (and external function no longer exists)
		mDebugLogDelegate = null;
		mOnFinishedDelegate = null;
		PopMovie_Free (mInstance);
		FlushDebug ();
	}
	
	public void UpdateTexture(RenderTexture Target)
	{
		Update ();
		PopMovie_UpdateRenderTexture (mInstance, TexturePtrCache.GetCache( ref mRenderTexturePtrCache, Target ), Target.width, Target.height, Target.format );
		FlushDebug ();
	}
	
	public void UpdateTexture(Texture2D Target)
	{
		Update ();
		PopMovie_UpdateTexture2D (mInstance, TexturePtrCache.GetCache( ref mTexture2DPtrCache, Target ), Target.width, Target.height, Target.format );
		FlushDebug ();
	}
	
	public void SetTime(float TimeSecs)
	{
		SetTime ((ulong)(TimeSecs * 1000.0f));
	}
	
	public void SetTime(ulong TimeMs)
	{
		//	update time
		try {
			if ( IsAllocated () )
			{
				PopMovie_SetTime ( mInstance, TimeMs);

				//	check if we've finished to trigger the callback
				if ( mOnFinishedDelegate != null )
				{
					ulong DurationMs = GetDurationMs();
					//	duration of 0 = unknown
					if ( TimeMs >= DurationMs && DurationMs > 0 )
						mOnFinishedDelegate();
				}
			}
		} catch {
		}
	}
	
	public ulong GetDurationMs()
	{
		ulong DurationMs = 0;
		try {
			DurationMs = PopMovie_GetDurationMs (mInstance);
		} catch {
			Debug.LogWarning ("Exception thrown getting duration (old/missing plugin?)");
		}
	
		return DurationMs;
	}

	public float GetDuration()
	{
		ulong DurationMs = GetDurationMs ();

		//	convert to float
		float DurationSec = (float)DurationMs / 1000.0f;
		return DurationSec;
	}

	void FlushDebug()
	{
		FlushDebug (mDebugLogDelegate);
	}

	void FlushDebug(DebugLogDelegate Callback)
	{
		//	if we have no listeners, do fast flush
		bool HasListeners = (Callback != null) && (Callback.GetInvocationList ().Length > 0);
		if (HasListeners) {
			//	IOS (and aot-only platforms cannot get a function pointer. Find a workaround!
#if UNITY_IOS && !UNITY_EDITOR
			FlushDebug (System.IntPtr.Zero);
#else
			FlushDebug (Marshal.GetFunctionPointerForDelegate (Callback));
#endif
		} else {
			FlushDebug (System.IntPtr.Zero);
		}
	}
	
	
	public void Update()
	{
		//	if auto playing... update time
		if ( mStartTime >= 0 )
			SetTime( Time.time - mStartTime );

		GL.IssuePluginEvent (mPluginEventId);
		FlushDebug();
	}
	
	static bool StringStartsWith(string String,string[] Exclusions)
	{
		foreach ( var e in Exclusions )
		{
			if ( String.StartsWith(e) )
				return true;
		}
		return false;
	}
	
	//	copy from streaming assets to persistent data so android can read it
	string GetVideoFilename(string AssetFilename)
	{
		//	if the filename has a protocol, don't change it
		//	device:Name		Internal device. webcam, camera etc. Put in a wrong name to get a debug list of availible options
		//	apk:Filename	Android: stream from the /assets/ folder in the APK (this is where streaming assets are placed)
		//	test:			Test decoder
		//	sdcard:			Android: Find the file in the external storage. Can be done in Unity (check System.IO.File.Exists()) but there is NO CONSISTENT path to sdcard

		//	skip these protocols as we'll use WWW to download them
		string[] WwwProtocols = new string[2] {"http:", "https:" };
		bool UseWWW = StringStartsWith( AssetFilename, WwwProtocols );

		if (AssetFilename.Contains (":") && !UseWWW) {
			return AssetFilename;
		}

		//	if file exists, assume raw path will work.
		//	on android, this WORKS for persistent(downloaded data), but will FAIL for streaming assets
		if ( System.IO.File.Exists (AssetFilename)) {
			return AssetFilename;
		}

		//	gr: this doesn't work on android;
		//	http://answers.unity3d.com/questions/210909/android-streamingassets-file-access.html
		//	so currently we can't tell if this is a special file... ASSUMING it's in the streaming assets folder.
#if UNITY_ANDROID && !UNITY_EDITOR
		return "apk:" + AssetFilename;
#endif

		//	if the filename exists in streaming assets, return the prefixed version
		String StreamingFilename = System.IO.Path.Combine(Application.streamingAssetsPath, AssetFilename);
		if (System.IO.File.Exists (StreamingFilename)) {
			//	other platforms just need the full path
			return StreamingFilename;
		}

		//	try downloading with WWW
		//	gr: disabled atm. Unity unstable with file persistent+url filenames
		/*
		if ( UseWWW )
		{
			try
			{
				String PersistFilename = System.IO.Path.Combine(Application.persistentDataPath, AssetFilename);

				//	persistent file already exists, skip the copy
				if (System.IO.File.Exists (PersistFilename))
				{
					DebugLog ("Persistent file already copied: " + PersistFilename);
					return PersistFilename;
				}

				WWW StreamingFile = new WWW(StreamingFilename);
				while ( !StreamingFile.isDone )
				{
					DebugLog("loading...." + StreamingFile.bytesDownloaded );
				}
				if ( StreamingFile.error != null )
				{
					throw new System.Exception( "www error: " + StreamingFile.error );
				}

				//DebugText.Log( "Writing to persistent: " + PersistFilename );
				System.IO.File.WriteAllBytes ( PersistFilename, StreamingFile.bytes );

				if (!System.IO.File.Exists(PersistFilename))
				{
					throw new System.Exception( "File does not exist after copy: " + PersistFilename );
				}

				return PersistFilename;
			}
			catch ( System.Exception e )
			{
				DebugLog("Error copying " + AssetFilename + " from WWW to persistent filename; " + e.ToString() + " " + e.Message  );
			}
		}
		*/
		//	hope for the best!
		return AssetFilename;
	}


}

