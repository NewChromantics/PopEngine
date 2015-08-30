using UnityEngine;
using System.Collections;
using System.Collections.Generic;
using System.IO;


public class Tuple<T1, T2>
{
	public T1 Key { get; set; }
	public T2 Value { get; set; }

	public Tuple(T1 Key, T2 Value)
	{
		this.Key = Key;
		this.Value = Value;
	}
}

public class TRectAtlasFrame
{
	public long			mGenerationTime;
	public long			mTimecode;
	public List<Rect>	mRects;
	public int			mTextureDataStart;
	public int			mTextureDataLength;
};

public class RectStreamParser : MonoBehaviour {

	public string					mRectAtlasFilename = "mancity01.rectatlasstream";
	private List<TRectAtlasFrame>	mFrames;

	private FileStream				mParseStream;
	private BinaryReader			mReader;
	private FileStream				mLoadTextureStream;

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

	List<Rect> ParseRects(string RectsString)
	{
		List<Rect> Rects = new List<Rect> ();
		return Rects;
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

			if (Variable.Key == "PopTrack") {
				Frame.mGenerationTime = ParseTimecode(Variable.Value);
				continue;
			}
			if (Variable.Key == "Rects") {
				Frame.mRects = ParseRects(Variable.Value);
				continue;
			}
			if (Variable.Key == "Frame") {
				Frame.mTimecode = ParseTimecode(Variable.Value);
				continue;
			}
			if (Variable.Key == "PixelsSize") {
				Frame.mTextureDataLength = int.Parse (Variable.Value);
				Frame.mTextureDataStart = (int)Reader.BaseStream.Position;
				//continue;
				break;
			}
			throw new System.Exception("Unknown variable " + Variable.Key);
		}

		//	walk over texture data
		Reader.BaseStream.Position += Frame.mTextureDataLength;

		return Frame;
	}

	public bool LoadMaskTexture(Texture2D Tex,float TimeSecs)
	{
		int FrameIndex = FindNearestFrameIndex (TimeSecs);
		if (FrameIndex == -1)
			return false;

		TRectAtlasFrame Frame = mFrames [FrameIndex];
		if ( mLoadTextureStream == null )
		{
			string StreamingFileanme = System.IO.Path.Combine (Application.streamingAssetsPath, mRectAtlasFilename);
			mLoadTextureStream = new FileStream (StreamingFileanme, FileMode.Open, FileAccess.Read, FileShare.Read );
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
