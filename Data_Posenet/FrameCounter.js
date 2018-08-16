

function PopFrameRateCounter()
{
	this.LastFrameRateTimelapse = Date.now();
	this.FrameCounters = [];
	
	this.CheckFrameRateLapse = function()
	{
		let Now = Date.now();
		if ( Now - this.LastFrameRateTimelapse < 1000 )
			return;
		//	1 sec has lapsed
		//	ideally timelapsed = 1
		let TimeLapsed = (Now - this.LastFrameRateTimelapse) / 1000;
		
		let This = this;
		let UpdateCounter = function(CounterName)
		{
			Debug("UpdateCounter");
			let Count = This.FrameCounters[CounterName];
			This.FrameCounters[CounterName] = 0;
			let Fps = Count / TimeLapsed;
			Debug( CounterName + " " + Fps.toFixed(2) + "fps");
		}
		let CounterNames = Object.keys(this.FrameCounters);
		CounterNames.forEach( UpdateCounter );
		this.LastFrameRateTimelapse = Now;
	}
	
	this.UpdateFrameCounter = function(CounterName)
	{
		if ( this.FrameCounters[CounterName] === undefined )
			this.FrameCounters[CounterName] = 0;
		this.FrameCounters[CounterName]++;
		this.CheckFrameRateLapse();
	}
		
}


var _FrameRateCounter = new PopFrameRateCounter();
function UpdateFrameCounter(CounterName)
{
	_FrameRateCounter.UpdateFrameCounter(CounterName);
}

