import Foundation




@objc public class PopEngineLabelWrapper : PopEngineLabel , ObservableObject
{
	//	called from objective-c's .label setter
	@objc override func updateUi() 
	{
		labelCopy = self.label
	}
	
	@Published var labelCopy:String = "LabelCopy"
}
