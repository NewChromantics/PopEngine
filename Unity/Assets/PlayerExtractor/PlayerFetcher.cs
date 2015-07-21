using UnityEngine;
using System.Collections;
using System.Collections.Generic;

[ExecuteInEditMode]
public class PlayerFetcher : MonoBehaviour {

	public int			mTime = 1001;
	private int			mPendingDataTime = -1;
	private int			mCurrentPointDataTime = 0;
	public PointViewer	mPointViewer;
	public string		mServerAddress = "http://localhost:8080/run/?";
	public string		mFilterName = "input01";

	void Update()
	{
		if (!mPointViewer)
			return;

		if (mTime != mCurrentPointDataTime && mPendingDataTime == -1 ) {
			StartCoroutine ("FetchPlayers");
		}
	}

	void ParsePlayerRects(string PlayerRects)
	{
		//	first line is debug
		var Lines = PlayerRects.Split (new char[]{'\n'});
		if (Lines.Length == 0) {
			Debug.LogWarning ("Extracting player rects had no lines");
			return;
		}

		Debug.Log (Lines [0]);

		List<Vector2> NewPoints = new List<Vector2> ();

		//	parse coords
		var RectStrings = Lines [1].Split (',');
		foreach (var RectString in RectStrings) {
			var Coords = RectString.Split ('x');
			Rect rect = new Rect (float.Parse (Coords [0]), float.Parse (Coords [1]), float.Parse (Coords [2]), float.Parse (Coords [3]));
			NewPoints.Add( new Vector2( rect.center.x, rect.max.y ) );
		}

		mPointViewer.mPoints = NewPoints.ToArray();
	}

	IEnumerator FetchPlayers()
	{
		mPendingDataTime = mTime;

		Debug.Log ("Fetching players for " + mPendingDataTime);

		string Url = mServerAddress;
		Url += "filter" + "=" + mFilterName + "&";

		Url += "time=" + mTime + "&";

		WWW www = new WWW (Url);
		yield return www;
		
		if (www.error != null)
			Debug.LogWarning ("Fetch players: " + www.error);

		if (www.text != null && www.text.Length > 0) 
		{
			try
			{
				ParsePlayerRects (www.text);
			}
			catch(System.Exception e)
			{
				Debug.LogWarning("Failed to parse www response: " + e.Message);
			}

		}

		mCurrentPointDataTime = mPendingDataTime; 
		mPendingDataTime = -1;
		

		www = null;
		yield break;
	}
}
