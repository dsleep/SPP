package com.sleepingrobot.androidtipsapp;

import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;

import android.widget.TextView;
import android.widget.Spinner;
import android.widget.LinearLayout;
import android.widget.Button;
import java.util.ArrayList;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.os.Bundle;
import android.util.Log;
import java.io.*;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothServerSocket;
import android.bluetooth.BluetoothSocket;
import android.view.View;
import android.view.VelocityTracker;
import java.util.*;
import android.view.MotionEvent;
import org.json.*;

public class MainActivity extends AppCompatActivity implements View.OnClickListener {

    private BluetoothAdapter mAdapter = null;
    private Spinner btChoices = null;
    private Button doConnect = null;
    private BluetoothSocket socketConnection = null;
    private BluetoothClient connectedThread = null;
    private static final UUID BT_MODULE_UUID = UUID.fromString("263beec5-a7fe-443a-a9ee-9bdfc5fc17a3");
    private static final String TAG = "BT";
    private VelocityTracker mVelocityTracker = null;

    private TextView tv = null;


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        //setContentView(R.layout.activity_main);

        Log.d(TAG, "AndroidBTTest");

        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        /* Create a TextView and set its text to "Hello world" */
        tv = new TextView(this);
        tv.setText("BT TEST!!");

        doConnect = new Button(this);
        doConnect.setText("Connect");
        doConnect.setOnClickListener(this);

        layout.addView(tv);

        setContentView(layout);

        btChoices = new Spinner(this);

        // Spinner Drop down elements
        List<String> BTNames = new ArrayList<String>();

        mAdapter = BluetoothAdapter.getDefaultAdapter();

        if (!mAdapter.isEnabled())
        {
            //Intent enableBtIntent = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
            //startActivityForResult(enableBtIntent, REQUEST_ENABLE_BT);
        }
        else
        {
            Set<BluetoothDevice> pairedDevices = mAdapter.getBondedDevices();

            // There are paired devices. Get the name and address of each paired device.
            for (BluetoothDevice device : pairedDevices)
            {
                String deviceName = device.getName();
                String deviceHardwareAddress = device.getAddress(); // MAC address

                BTNames.add(deviceName + ":" + deviceHardwareAddress);

                Log.d(TAG, "Device Name: " + deviceName);
                Log.d(TAG, "Device Addr: " + deviceHardwareAddress);
            }

            ArrayAdapter<String> dataAdapter = new ArrayAdapter<String>(this,
                    android.R.layout.simple_spinner_item,
                    BTNames);

            // attaching data adapter to spinner
            btChoices.setAdapter(dataAdapter);

            layout.addView(btChoices);
        }

        layout.addView(doConnect);
    }

    @Override
    public void onClick(View v)
    {
        String text = btChoices.getSelectedItem().toString();

        Log.d(TAG, "************************");
        Log.d(TAG, "************************");
        Log.d(TAG, "************************");
        Log.d(TAG, "CONNECT TO: " + text);

        final String address = text.substring(text.length() - 17);
        BluetoothDevice device = mAdapter.getRemoteDevice(address);

        Log.d(TAG, " - address: " + address);
        Log.d(TAG, " - GUID: " + BT_MODULE_UUID);

        if(connectedThread != null)
        {
            connectedThread.stop();
            connectedThread = null;
        }
        if(socketConnection != null)
        {
            try
            {
                socketConnection.close();
                socketConnection = null;
            } catch (IOException e) {
            }
        }

        try {
            socketConnection = createBluetoothSocket(device);
            Log.d(TAG, " - created socket: " + socketConnection);
        } catch (IOException e) {
            Log.d(TAG, " - SOCKET FAILED");
            return;
        }
        try {
            Log.d(TAG, " - called connect: " + socketConnection);
            socketConnection.connect();
        } catch (IOException e) {
            Log.d(TAG, " - CONNECT FAILED");
            try {
                socketConnection.close();
            } catch (IOException e2) {
                //insert code to deal with this
            }
            return;
        }

        tv.setText("CONNECTED");

        connectedThread = new BluetoothClient(socketConnection, null);
        connectedThread.start();
        Log.i(TAG, " - CONNECTING");
    }

    private BluetoothSocket createBluetoothSocket(BluetoothDevice device) throws IOException {
        return device.createRfcommSocketToServiceRecord(BT_MODULE_UUID);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        int action = event.getActionMasked();

        switch(action) {
            case MotionEvent.ACTION_DOWN:

                if(connectedThread!=null)
                {
                    JSONObject obj = new JSONObject();
                    try
                    {
                        obj.put("X", event.getX());
                        obj.put("Y", event.getY());
                    }
                    catch (JSONException e)  {}
                    connectedThread.write(obj.toString());
                }

                break;
            case MotionEvent.ACTION_MOVE:
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                break;
        }

        return true;
    }
}