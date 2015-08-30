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



[StructLayout(LayoutKind.Explicit, Pack=1, Size=228)]
public class  PopMovieParams
{
[FieldOffset(0)]
[MarshalAs(UnmanagedType.ByValTStr, SizeConst = 200)]
public string			mFilename;

[FieldOffset(200)]
public ulong			mPreSeekMs = 0;						//	start decoding at a particular point in video. Can be used to pre-buffer to aid synchronisation

//	enum appears to be 32 bit in c++. Check for overwriting params
[FieldOffset(208)]
public TextureFormat	mDecodeAsFormat = TextureFormat.RGBA32;	//	what format to decode as (some platforms limit this). Best to try and match format of target texture and source video to reduce colour conversion

[FieldOffset(212)]
public bool				mSkipPushFrames = false;			//	skip queuing old frames after decoding if in the past
[FieldOffset(213)]
public bool				mSkipPopFrames = true;				//	skip old frames when possible when presenting latest (synchronisation)
[FieldOffset(214)]
public bool				mAllowGpuColourConversion = true;	//	allow colour conversion on graphics driver - often done in CPU (depends on platform/driver)
[FieldOffset(215)]
public bool				mAllowCpuColourConversion = false;	//	allow internal colour conversion on CPU. Slow!
[FieldOffset(216)]
public bool				mAllowSlowCopy = true;				//	allow CPU->GPU pixel copying
[FieldOffset(217)]
public bool				mAllowFastCopy = true;				//	allow GPU->GPU copying where availible
[FieldOffset(218)]
public bool				mPixelClientStorage = false;		//	on OSX allow storing pixels on host/cpu rather than uploading to GPU (faster CPU copy)
[FieldOffset(219)]
public bool				mDebugFrameSkipping = true;			//	print out messages when we drop/skip frames for various reasons
[FieldOffset(220)]
public bool				mPeekBeforeDefferedCopy = true;		//	don't queue a copy (for when we do copies on the graphics thread) if RIGHT NOW there is no new frame. Can reduce wasted time on graphics thread
[FieldOffset(221)]
public bool				mDecodeAsYuvFullRange = false;		//	decode as YUV (full range in luma channel)
[FieldOffset(222)]
public bool				mDecodeAsYuvVideo = false;			//	decode as YUV (SDTV range in luma channel)
[FieldOffset(223)]
public bool				mDebugNoNewPixelBuffer = false;		//	print out when there is no frame to pop. Useful if frames aren't appearing with no error
[FieldOffset(224)]
public bool				mDebugRenderThreadCallback = false;	//	turn on to show that unity is calling the plugin's graphics thread callback (essential for multithreaded rendering, and often is a problem with staticcly linked plugins - like ios)
[FieldOffset(225)]
public bool				mResetInternalTimestamp = true;		//	if your source video's timestamps don't start at 0, this resets them so the first frame becomes 0
[FieldOffset(226)]
public bool				mDebugBlit = false;					//	use the test shader for blitting
[FieldOffset(227)]
public bool				mApplyVideoTransform = true;		//	apply the transform found inside the video
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

#if UNITY_STANDALONE_OSX || UNITY_EDITOR_OSX
	static TextureFormat		mInternalFormat = TextureFormat.RGBA32;
#elif UNITY_STANDALONE_WIN || UNITY_EDITOR_WIN
	static TextureFormat		mInternalFormat = TextureFormat.RGB24;
#elif UNITY_IOS
	static TextureFormat		mInternalFormat = TextureFormat.BGRA32;
#elif UNITY_ANDROID
	static TextureFormat		mInternalFormat = TextureFormat.RGBA32;
#endif

    private ulong							mInstance = 0;
	private static int						mPluginEventId = PopMovie_GetPluginEventId();

	//	cache the texture ptr's. Unity docs say accessing them causes a GPU sync, I don't believe they do, BUT we want to avoid setting the active render texture anyway
	private TTexturePtrCache<Texture2D>		mTexture2DPtrCache = new TTexturePtrCache<Texture2D>();
	private TTexturePtrCache<RenderTexture>	mRenderTexturePtrCache = new TTexturePtrCache<RenderTexture>();

	[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
	public delegate void DebugLogDelegate(string str);
	private DebugLogDelegate	mDebugLogDelegate = null;
	
	[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
	public delegate void OpenglCallbackDelegate();

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
	private static extern ulong		PopMovie_Alloc(PopMovieParams Params);

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


	public PopMovie(string Filename,PopMovieParams Params)
	{
#if UNITY_EDITOR && UNITY_IOS
		UnityEditor.ScriptingImplementation backend = (UnityEditor.ScriptingImplementation)UnityEditor.PlayerSettings.GetPropertyInt("ScriptingBackend", UnityEditor.BuildTargetGroup.iOS);
		if (backend != UnityEditor.ScriptingImplementation.IL2CPP) {
			Debug.LogWarning ("Warning: If the scripting backend is not IL2CPP on IOS there may be problems at runtime passing parameters to the plugin.");
		}
#endif
		string FinalFilename = GetVideoFilename (Filename);

		Params.mDecodeAsFormat = mInternalFormat;
		Params.mFilename = FinalFilename;

		mInstance = PopMovie_Alloc ( Params );

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
	}
	
	~PopMovie()
	{
		//	gr: don't quite get the destruction order here, but need to remove the [external] delegates in destructor. 
		//	Assuming external delegate has been deleted, and this garbage collection (despite being explicitly called) 
		//	is still deffered until after parent object[monobehaviour] has been destroyed (and external function no longer exists)
		mDebugLogDelegate = null;
		PopMovie_Free (mInstance);
		FlushDebug ();
	}
	
	public bool UpdateTexture(RenderTexture Target,ref String Error)
	{
		bool Success = false;
		try
		{
			Success = PopMovie_UpdateRenderTexture (mInstance, TexturePtrCache.GetCache( ref mRenderTexturePtrCache, Target ), Target.width, Target.height, Target.format );
			if ( !Success )
				Error = "Plugin error";
		}
		catch ( System.Exception e )
		{
			Error =  e.ToString() + " " + e.Message ;
		}
		FlushDebug ();
		return Success;
	}
	
	public bool UpdateTexture(Texture2D Target,ref String Error)
	{
		bool Success = false;
		try
		{
			Success = PopMovie_UpdateTexture2D (mInstance, TexturePtrCache.GetCache( ref mTexture2DPtrCache, Target ), Target.width, Target.height, Target.format );
			if ( !Success )
				Error = "Plugin error";
		}
		catch ( System.Exception e )
		{
			Error =  e.ToString() + " " + e.Message ;
		}
		FlushDebug ();
		return Success;
	}
	
	public void SetTime(float TimeSecs)
	{
		SetTime ((ulong)(TimeSecs * 1000.0f));
	}
	
	public void SetTime(ulong TimeMs)
	{
		Update ();
		//	update time
		try {
			if ( IsAllocated () )
			{
				PopMovie_SetTime ( mInstance, TimeMs);
			}
		} catch {
		}
		Update ();
	}
	
	public float GetDuration()
	{
		ulong DurationMs = 0;
		try {
			DurationMs = PopMovie_GetDurationMs (mInstance);
		} catch {
			Debug.LogWarning ("Exception thrown getting duration (old plugin?)");
		}
		
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

