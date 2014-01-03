Pebble.addEventListener("ready",
  function(e) {
    App.post.get();
  }
);

Pebble.addEventListener("showConfiguration",
  function(e) {
    console.log("Showing Configuration");
    Pebble.openURL("html/configuration.html");
  }
);

Pebble.addEventListener("webviewclosed",
  function(e) {
    var configuration = JSON.parse(e.response);
    console.log("Configuration window returned: ", configuration);
  }
);

Pebble.addEventListener("appmessage",
  function(e) {
    console.log("Received message: " + e.payload);
  }
);

// store functions in a global dict namespace
App = {};
// data objects for sending to Pebble
App.data = {};
App.data.queue = []; //queue of data dict to send
App.data.is_sending = false;
App.data.send = function(data) { // should be a dict
  App.data.is_sending = true;
  Pebble.sendAppMessage(data,
    function(e) {
      // ack - send next message in queue
      if (App.data.queue.length) {
        App.data.send(App.data.queue.shift());
      } else {
        App.data.is_sending = false;
      }
    },
    function(e) {
      // nack - send next message in queue
      console.error("nack sending story");
      if (App.data.queue.length) {
        App.data.send(App.data.queue.shift());
      } else {
        App.data.is_sending = false;
      }
  });
};
App.data.add_to_queue = function(data) {
  if (App.data.is_sending) {
    // currently sending - add to queue
    App.data.queue.push(data);
  } else {
    // send immediately
    App.data.send(data);
  }
};
// functions to get data about posts
App.post = {};
App.post.posts = [];
App.post.get = function() {
  var req = new XMLHttpRequest();
  req.open('GET', 'http://reddit.com/r/all.json', true);
  req.onload = function(e) {
    if (req.readyState == 4 && req.status == 200) {
      if(req.status == 200) {
        var response = JSON.parse(req.responseText);
        var num_children = response.data.children.length;
        for (var i=0; i<num_children; i++) {
          var post = response.data.children[i].data;
          App.post.posts.push(post);
          if (i<5) {
            // send the first five messages to the watch
            var data = {"post": 1, 
                        "index": i,
                        "subreddit": post.subreddit,
                        "author": post.author,
                        "title": post.title};
            App.data.add_to_queue(data);
          }
        }
      } else { console.log("Error"); }
    }
  };
  req.send(null);
};

