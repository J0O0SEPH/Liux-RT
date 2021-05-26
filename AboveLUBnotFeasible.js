/*
    Javascript snippts used for google-sheets manual anaylsis.
    each function assign 1 to the time slot acquiring the processor.
*/

function SetService2() 
{
  var app = SpreadsheetApp;
  var SS = app.getActiveSpreadsheet();
  var Sheet = SS.getActiveSheet();
  var i;

  for(i = 2; i < 912; i++)
  {
    Sheet.getRange(11,i).setValue(0);
  }
  for(i = 2; i < 912; i+=5)
  {
    var j; var sum = 0; var filled = false;
    for(j = i; j < i+5; j++)
    {
      if(Sheet.getRange(9, j).getValue()==0 && !filled)
      {
        Sheet.getRange(11,j).setValue(1);
        filled = true;
      }
    }
  }
}


function SetService3() 
{
    var app = SpreadsheetApp;
    var SS = app.getActiveSpreadsheet();
    var Sheet = SS.getActiveSheet();
    var i;
  
    for(i = 2; i < 912; i++)
    {
      Sheet.getRange(13,i).setValue(0);
    }
    for(i = 2; i < 912; i+=7)
    {
      var j; var filled = false;
      for(j = i; j < i+7; j++)
      {
        if(Sheet.getRange(9, j).getValue()==0 && Sheet.getRange(11, j).getValue()==0 && !filled)
        {
          Sheet.getRange(13,j).setValue(1);
          filled = true;
        }
      }
    }
}


function SetService4() 
{
    var app = SpreadsheetApp;
    var SS = app.getActiveSpreadsheet();
    var Sheet = SS.getActiveSheet();
    var i;
  
    for(i = 2; i < 912; i++)
    {
      Sheet.getRange(15,i).setValue(0);
    }
    for(i = 2; i < 912; i+=13)
    {
      var j; var filled = 2;
      for(j = i; j < i+13; j++)
      {
        if(
           Sheet.getRange(9, j).getValue()==0 && 
           Sheet.getRange(11, j).getValue()==0 && 
           Sheet.getRange(13, j).getValue()==0 && 
           filled > 0
          )
        {
          Sheet.getRange(15,j).setValue(1);
          filled--;
        }
      }
    }
}

function Slack() 
{
    var app = SpreadsheetApp;
    var SS = app.getActiveSpreadsheet();
    var Sheet = SS.getActiveSheet();
    var i;
  
    for(i = 2; i < 912; i++)
    {
      Sheet.getRange(16,i).setValue(0);
    }
    for(i = 2; i < 912; i++)
    {
      if(
          Sheet.getRange(9, i).getValue()==0 && 
          Sheet.getRange(11, i).getValue()==0 && 
          Sheet.getRange(13, i).getValue()==0 && 
          Sheet.getRange(15, i).getValue()==0
        )
      {
        Sheet.getRange(16,i).setValue(1);
      }
    }
}

function Missed() 
{
    var app = SpreadsheetApp;
    var SS = app.getActiveSpreadsheet();
    var Sheet = SS.getActiveSheet();
    var i;
  
    for(i = 2; i < 912; i+=13)
    {
      var j; var filled = 0;
      for(j = i; j < i+13; j++)
      {
        if(Sheet.getRange(15, j).getValue()==1)
        {
          filled++;
        }
      }
      if(filled < 2)
      {
        Sheet.getRange(15, i+12).setValue(2);
      }
    }
}