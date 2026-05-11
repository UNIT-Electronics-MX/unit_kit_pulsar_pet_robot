#ifndef WEBPAGE_H
#define WEBPAGE_H

const char index_html[] PROGMEM = R"rawliteral(

<!DOCTYPE html>
<html>

<head>

<meta name="viewport" content="width=device-width, initial-scale=1">

<title>Perrito Robot</title>

<style>

body{
    background:#101010;
    color:white;
    font-family:Arial;
    text-align:center;
    margin:0;
    padding:20px;
}

h1{
    margin-bottom:30px;
}

.section{
    margin-bottom:35px;
}

button{

    width:220px;
    height:60px;

    margin:10px;

    border:none;
    border-radius:18px;

    background:#2d2d2d;
    color:white;

    font-size:20px;

    transition:0.2s;
}

button:active{
    transform:scale(0.96);
    background:#444;
}

</style>

</head>

<body>

<h1>UNIT Pulsar Pet Robot</h1>

<div class="section">

<h2>Emotions</h2>

<button onclick="fetch('/happy')">
  Happy
</button>

<button onclick="fetch('/sleepy')">
  Sleepy
</button>

<button onclick="fetch('/surprised')">
  Surprised
</button>

<button onclick="fetch('/wink')">
  Wink
</button>

<button onclick="fetch('/angry')">
  Angry
</button>

</div>

<div class="section">

<h2>Actions</h2>
<button onclick="fetch('/standup')">
  StandUP
</button>

<button onclick="fetch('/sentarse')">
  Sit
</button>

<button onclick="fetch('/dormir')">
  Sleep
</button>

<button onclick="fetch('/saludar')">
  Hi five
</button>

<button onclick="fetch('/walk')">
  Walk
</button>

<button onclick="fetch('/stop')">
Stop
</button>

</div>

</body>
</html>

)rawliteral";

#endif