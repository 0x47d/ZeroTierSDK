﻿using UnityEngine;
using System.Collections;
using System.Threading;
using System.Runtime.Serialization.Formatters.Binary;
using System.IO;
using UnityEngine.UI;

public class Demo : MonoBehaviour
{
	public float speed = 300f;

	private ZeroTierNetworkInterface zt;
	string nwid = "";

	private void zt_sample_network_test_thread()
	{
		print("test_network");

		byte error;
		// Prepare sample data buffer
		/*
		byte[] buffer = new byte[1024];
		Stream stream = new MemoryStream(buffer);
		BinaryFormatter f = new BinaryFormatter();
		f.Serialize ( stream , "Welcome to the machine! (from Unity3D)" );
		int error;
		*/

		// Connect to server
		int connfd = zt.Connect (0, "172.22.211.245", 8888, out error);
		print(connfd);

		// Send sample data to server
		//int bytes_written = zt.Send(connfd,buffer,0, out error);
		//print(bytes_written);

		char[] buffer = new char[1024];
		buffer = "hello".ToCharArray();
		//print (buffer);
		//Stream stream = new MemoryStream(buffer);
		//BinaryFormatter formatter = new BinaryFormatter();
		//formatter.Serialize(stream, "HelloServer");
		//int bufferSize = 1024;

		int bytes_written = zt.Send(connfd, "hello".ToCharArray(),4, out error);
		print(bytes_written);
	}

	public void zt_test_network()
	{
		Thread networkTestThread = new Thread(() => { zt_sample_network_test_thread();});
		networkTestThread.IsBackground = true;
		networkTestThread.Start();
	}

	// Demo button methods
	public void Join()
	{
		GameObject go = GameObject.Find ("inputNetworkID"); 
		InputField input = go.GetComponents<InputField> () [0];
		Debug.Log ("Joining: " + input.text);
		zt.JoinNetwork (input.text);
	}

	public void Leave()
	{
		GameObject go = GameObject.Find ("inputNetworkID"); 
		InputField input = go.GetComponents<InputField> () [0];
		Debug.Log ("Leaving: " + input.text);
		zt.LeaveNetwork (input.text);
	}


	public void Connect()
	{
		GameObject go = GameObject.Find ("inputServerAddress"); 
		//Text text = go.GetComponents<Text> ()[0];
		InputField input = go.GetComponents<InputField> () [0];
		Debug.Log ("Connecting to: " + input.text);
		//zt.Connect(0, input.text
	}

	public void Disconnect()
	{
		GameObject go = GameObject.Find ("inputServerAddress"); 
		InputField text = go.GetComponents<InputField> () [0];
		Debug.Log ("Disconnecting from: " + text.text);
	}


	public void SendMessage()
	{
		GameObject go = GameObject.Find ("inputMessage"); 
		InputField text = go.GetComponents<InputField> () [0];
		Debug.Log ("Sending Message: " + text.text);
	}


	void Start()
	{
		// Set defaults
		InputField input;
		GameObject go;
		go = GameObject.Find ("inputNetworkID"); 
		input = go.GetComponents<InputField> () [0];
		input.text = "565799d8f6e1c11a";
		go = GameObject.Find ("inputServerAddress"); 
		input = go.GetComponents<InputField> () [0];
		input.text = "172.22.211.245";
		go = GameObject.Find ("inputMessage"); 
		input = go.GetComponents<InputField> () [0];
		input.text = "Welcome to the machine";

		// Create new instance of ZeroTier in separate thread
		zt = new ZeroTierNetworkInterface ("/Users/Joseph/utest2");

		/* This new instance will communicate via a named pipe, so any 
		 * API calls (ZeroTier.Connect(), ZeroTier.Send(), etc) will be sent to the service
		 * via this pipe.
		 */
	}

	// Terminate the ZeroTier service when the application quits
	void OnApplicationQuit() { 
		zt.Terminate ();
	} 

	// Update is called once per frame
	void Update () {

		// Rotate ZTCube when ZT is running
		/*
		if (zt.IsRunning ()) {
			GameObject go = GameObject.Find ("ZTCube"); 
			Vector3 rotvec = new Vector3 (10f, 10f, 10f);
			go.transform.Rotate (rotvec, speed * Time.deltaTime);
		}
		*/
		/*
		GameObject go = GameObject.Find("ZTCube"); 
		Text text = go.GetComponents<Text> ()[0];
		if (text) {
			text.text = IsRunning() ? "ZeroTier Status: Online" : "ZeroTier Status: Offline";
		}
		*/
	}
}