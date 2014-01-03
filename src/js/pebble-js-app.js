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
    if ("req_post_data" in e.payload) {
      var index = e.payload.index;
      var post = App.post.posts[index];
      if (post == undefined) {
        console.error("Requested data for post index that doesn't exist: "+index);
      }
      // look for selftext in post using index
      if (post.selftext) {
        // return selftext
        console.log("Returning selftext for post: "+post.title);
        var data = {"req_post_data": 1,
                    "index": e.index,
                    "data": post.selftext.slice(0, 1024)};
        App.data.add_to_queue(data);
      } else {
        // if selftext doesn't exists, request the actual page and load it.
        console.log("Requesting url: "+post.url+ "\nfor post: "+post.title);
        var req = new XMLHttpRequest();
        req.open('GET', post.url, true);
        req.onload = function(e) {
          if (req.readyState == 4 && req.status == 200) {
            if(req.status == 200) {
              var data = {"post_data": 1,
                          "index": index,
                          "data": req.responseText.slice(0, 1024)};
              App.data.add_to_queue(data);
            } else { console.error("Failed to open url: "+post.url); }
          }
        };
        req.send(null);
      }
    }
  }
);

// store functions in a global dict namespace
App = {};
// data objects for sending to Pebble
App.data = {};
App.data.queue = []; //queue of data dict to send
App.data.is_sending = false;
App.data._on_send_ack = function(e) {
  // ack - send next message in queue
  if (App.data.queue.length) {
    App.data.send(App.data.queue.shift());
  } else {
    App.data.is_sending = false;
  }    
};
App.data.send = function(data) { // should be a dict
  App.data.is_sending = true;
  Pebble.sendAppMessage(data, 
                        App.data._on_send_ack,
                        App.data._on_send_ack);
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
          if (i<10) {
            // send the first ten messages to the watch
            var data = {"post": 1, 
                        "index": i,
                        "subreddit": post.subreddit,
                        "author": post.author,
                        "title": post.title};
            App.data.add_to_queue(data);
          }
        }
      } else { console.error("Error requesting posts from Reddit"); }
    }
  };
  req.send(null);
};

