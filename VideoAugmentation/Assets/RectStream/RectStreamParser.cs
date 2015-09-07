using UnityEngine;
using System.Collections;
using System.Collections.Generic;
using System.IO;


public class Tuple<T1, T2>
{
	public T1 First { get; set; }
	public T2 Second { get; set; }

	public Tuple(T1 First,T2 Second)
	{
		this.First = First;
		this.Second = Second;
	}
}

public class TRectAtlasFrame
{
	public long				mGenerationTime = 0;
	public long				mTimecode = 0;
	public List<Tuple<Rect,Rect>>	mRects;
	public int				mTextureDataStart = 0;
	public int				mTextureDataLength = 0;
	public int				mTextureWidth = 0;
	public int				mTextureHeight = 0;
	public TextureFormat	mTextureFormat;
};

public class RectStreamParser : MonoBehaviour {

	public string					mRectAtlasFilename = "mancity01.rectatlasstream";
	private List<TRectAtlasFrame>	mFrames;

	private FileStream				mParseStream;
	private BinaryReader			mReader;
	private FileStream				mLoadTextureStream;
	public int						mParsesPerFrame = 10;	//	rather than do ALL frames in one go

	bool FinishedParsing()
	{
		bool HasFrames = (mFrames!=null) && (mFrames.Count > 0);

		if (mReader == null && HasFrames)
			return true;
	
		return false;
	}

	void Update()
	{
		if (FinishedParsing ())
			return;

		//	first run
		if (mParseStream==null) {
			string StreamingFileanme = System.IO.Path.Combine (Application.streamingAssetsPath, mRectAtlasFilename);
			mParseStream = new FileStream (StreamingFileanme, FileMode.Open, FileAccess.Read, FileShare.Read );
		}
		if (mReader==null) {
			mReader = new BinaryReader (mParseStream);
		}

		for ( int i=0;	i<Mathf.Max (1,mParsesPerFrame);	i++ )
			ParseNextFrame ();
	}

	void ParseNextFrame()
	{
		var Frame = ParseFrame (mReader);

		//	failed to get frame... eof
		if (Frame == null) {
			mReader = null;
			mParseStream = null;
			return;
		}

		if (mFrames == null)
			mFrames = new List<TRectAtlasFrame> ();
		mFrames.Add (Frame);
		Debug.Log ("Read frame: " + Frame.mTimecode);
	}

	string PopString (BinaryReader Reader, char Delin)
	{
		string PoppedString = "";
		while ( true )
		{
			char Char = Reader.ReadChar();
			if ( Char == Delin )
				break;
			PoppedString += Char;
		}
		return PoppedString;
	}

	Tuple<string,string> PopVariable(BinaryReader Reader)
	{
		string Key = PopString (Reader, '=');
		string Value = PopString (Reader, ';');
		return new Tuple<string,string> (Key, Value);
	}

	long ParseTimecode(string Timecode)
	{
		//	pop T in T000000101
		if (Timecode[0]=='T') {
			Timecode = Timecode.Remove (0, 1);
		}

		try
		{
			long TimeMs = long.Parse (Timecode);
			return TimeMs;
		}
		catch( System.Exception e)
		{
			Debug.LogError("Failed to decode timecode; " +Timecode + " e: " + e.Message );
		}

		return 0;
	}
	
	public long FindNearestTimecode(float TimeSecs)
	{
		int NearestIndex = FindNearestFrameIndex (TimeSecs);
		if (NearestIndex == -1)
			return 0;
		return mFrames [NearestIndex].mTimecode;
	}

	private int FindNearestFrameIndex(float TimeSecs)
	{
		if ( mFrames==null || mFrames.Count == 0 )
			return -1;
		
		//	change to binary chop
		int TimeMs = (int)(TimeSecs * 1000.0f);
		
		for (int i=0; i<mFrames.Count; i++) {
			if (mFrames [i].mTimecode >= TimeMs) 
			{
				//	return previous frame, if there was one
				if ( i == 0 )
					return i;
				return i-1;
			}
		}
		
		//	Time is after all frames
		return mFrames.Count - 1;
	}

	Rect ParseRect(string RectStr)
	{
		char[] Delin = new char[]{','};
		string[] Floats = RectStr.Split (Delin);
		
		if (Floats.Length != 4)
			throw new System.Exception ("Rect-string \"" + RectStr + "\" didn't split into 4");

		float x = float.Parse (Floats [0]);
		float y = float.Parse (Floats [1]);
		float w = float.Parse (Floats [2]);
		float h = float.Parse (Floats [3]);
		return new Rect (x, y, w, h);
	}

	Tuple<Rect,Rect> ParseRectPair(string RectPairStr)
	{
		char[] Delin = new char[]{'>'};
		string[] Rects = RectPairStr.Split (Delin);

		if ( Rects.Length != 2 )
			throw new System.Exception("Rect-pair \"" + RectPairStr + "\" didn't split");

		Rect SourceRect = ParseRect(Rects [0]);
		Rect DestRect = ParseRect(Rects [1]);
		return new Tuple<Rect,Rect> (SourceRect, DestRect);
	}

	List<Tuple<Rect,Rect>> ParseRects(string RectsString)
	{
		List<Tuple<Rect,Rect>> Rects = new List<Tuple<Rect,Rect>> ();

		char[] Delin = new char[]{'#'};
		string[] RectPairs = RectsString.Split (Delin);
		foreach (string RectPairStr in RectPairs) {
			if ( RectPairStr.Length == 0 )
				continue;
			var RectPair = ParseRectPair (RectPairStr);
			Rects.Add (RectPair);
		}

		return Rects;
	}

	void ParseTextureMeta(ref int Width,ref int Height,ref TextureFormat Format,string MetaString)
	{
		//	640x480^RGBA
		char[] Delin = new char[]{'x','^'};
		string[] MetaParts = MetaString.Split (Delin);

		Width = int.Parse (MetaParts [0]);
		Height = int.Parse (MetaParts [1]);

		string FormatStr = MetaParts [2];
		if (FormatStr == "RGBA")
			Format = TextureFormat.RGBA32;
		else if (FormatStr == "RGB")
			Format = TextureFormat.RGB24;
		else if (FormatStr == "Greyscale")
			Format = TextureFormat.Alpha8;
		else
			throw new System.Exception ("Don't know format " + FormatStr);
	}


	TRectAtlasFrame ParseFrame(BinaryReader Reader)
	{
		TRectAtlasFrame Frame = new TRectAtlasFrame ();
		while (true) {

			Tuple<string,string> Variable;
			try
			{
				Variable = PopVariable (Reader);
			}
			catch (System.Exception e)
			{
				//	eof
				return null;
			}

			if (Variable.First == "PopTrack") {
				Frame.mGenerationTime = ParseTimecode(Variable.Second);
				continue;
			}
			if (Variable.First == "Rects") {
				Frame.mRects = ParseRects(Variable.Second);
				continue;
			}
			if (Variable.First == "Frame") {
				Frame.mTimecode = ParseTimecode(Variable.Second);
				continue;
			}
			if (Variable.First == "PixelsSize") {
				Frame.mTextureDataLength = int.Parse (Variable.Second);
				Frame.mTextureDataStart = (int)Reader.BaseStream.Position;
				//continue;
				break;
			}
			if ( Variable.First == "PixelMeta" )
			{
				ParseTextureMeta( ref Frame.mTextureWidth, ref Frame.mTextureHeight, ref Frame.mTextureFormat, Variable.Second );
				continue;
			}
		    throw new System.Exception("Unknown variable " + Variable.First);
		}

		//	walk over texture data
		Reader.BaseStream.Position += Frame.mTextureDataLength;

		return Frame;
	}


	public TRectAtlasFrame GetFrame(float TimeSecs)
	{
		int FrameIndex = FindNearestFrameIndex (TimeSecs);
		if (FrameIndex == -1)
			return null;
		
		TRectAtlasFrame Frame = mFrames [FrameIndex];
		return Frame;
	}

	public bool LoadMaskTexture(Texture2D Tex,float TimeSecs)
	{
		return LoadMaskTexture ( Tex, GetFrame (TimeSecs));
	}

	public bool LoadMaskTexture(Texture2D Tex,TRectAtlasFrame Frame)
	{
		if (Frame == null)
			return false;

		if ( mLoadTextureStream == null )
		{
			string StreamingFileanme = System.IO.Path.Combine (Application.streamingAssetsPath, mRectAtlasFilename);
			mLoadTextureStream = new FileStream (StreamingFileanme, FileMode.Open, FileAccess.Read, FileShare.Read );
		}

		//	gr: find a better way,like re-creating the texture?
		if (Tex.width != Frame.mTextureWidth || Tex.height != Frame.mTextureHeight || Tex.format != Frame.mTextureFormat) {
			throw new System.Exception ("Texture format doesn't match stream source: " + Tex.width + "x" + Tex.height + "(" + Tex.format + ") vs " + Frame.mTextureWidth + "x" + Frame.mTextureHeight + "(" + Frame.mTextureFormat + ")");
		}

		//	gr: for some reason... unity wants ONE extra byte...
		byte[] TextureBytes = new byte[Frame.mTextureDataLength+1];
		mLoadTextureStream.Position = Frame.mTextureDataStart;
		mLoadTextureStream.Read( TextureBytes, 0, Frame.mTextureDataLength );


		try 
		{
			Tex.LoadRawTextureData (TextureBytes);
			Tex.Apply ();
			return true;
		}
		catch ( System.Exception e )
		{
			byte[] CurrentBytes = Tex.GetRawTextureData();
			Debug.LogError("Failed to load raw texture data " + TextureBytes.Length + " bytes vs " + CurrentBytes.Length + " bytes. " + e.Message);
			return false;
		}
	}
}
