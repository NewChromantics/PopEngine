using UnityEngine;
using System.Collections;
using System.Collections.Generic;


[ExecuteInEditMode]
public class PointEditor : PointViewer {

	public float 	mMinGrabDistancePx = 10;

	virtual public void OnPointsChanged()
	{
	}

	Vector2? ScreenToView(Vector2 ScreenPos)
	{
		//	normalise
		ScreenPos.x /= Screen.width;
		ScreenPos.y = Screen.height - ScreenPos.y;
		ScreenPos.y /= Screen.height;
		
		ScreenPos.x -= mEditorRect.x;
		ScreenPos.y -= mEditorRect.y;
		ScreenPos.x /= mEditorRect.width;
		ScreenPos.y /= mEditorRect.height;
		
		
		if (ScreenPos.x < 0 || ScreenPos.x > 1 || ScreenPos.y < 0 || ScreenPos.y > 1)
			return null;
		
		return ScreenPos;
	}

	void MoveNearestPoint(Vector2 NewPos)
	{
		int NearestParam = -1;
		float NearestParamDistance = 9999;
		float MinGrabDistanceView = mMinGrabDistancePx / mEditorRect.width;
		
		for ( int p=0;	p<mPoints.Length;	p++ )
		{
			var View2 = mPoints[p];

			float Dist = (View2 - NewPos).magnitude;
			if (Dist > NearestParamDistance)
				continue;
			if ( Dist > MinGrabDistanceView )
				continue;
			NearestParam = p;
			NearestParamDistance = Dist;
		}
		
		if (NearestParam != -1) {
			mPoints [NearestParam] = NewPos;
			OnPointsChanged ();
		}
	}


	// Update is called once per frame
	public void Update () {
	
		base.Update ();

		if (Input.GetMouseButton (0)) {
			//	get editor pos
			Vector2 ScreenPos = new Vector2 (Input.mousePosition.x, Input.mousePosition.y);
			Vector2? ViewPos = ScreenToView (ScreenPos);
			if ( ViewPos != null )
				MoveNearestPoint( ViewPos.Value );
		}

	}


}
