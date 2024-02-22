package com.sleepingrobot.androidtipsapp;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.content.*;
import androidx.core.app.ActivityCompat;

import android.os.Bundle;

import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;

import android.content.pm.*;
import android.widget.TextView;
import android.widget.Spinner;
import android.widget.Toast;
import android.widget.LinearLayout;
import android.widget.Button;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.os.Bundle;
import android.util.Log;
import java.io.*;

import java.nio.ByteOrder;

import android.bluetooth.BluetoothServerSocket;
import android.bluetooth.BluetoothSocket;

import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattServer;
import android.bluetooth.BluetoothGattServerCallback;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.le.AdvertiseCallback;
import android.bluetooth.le.AdvertiseData;
import android.bluetooth.le.AdvertiseSettings;
import android.bluetooth.le.BluetoothLeAdvertiser;

import android.view.WindowManager;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;

import android.os.ParcelUuid;

import android.view.View;
import android.view.VelocityTracker;
import java.util.*;
import android.view.MotionEvent;
import org.json.*;

public class MainActivity extends AppCompatActivity implements SensorEventListener  {


    private static final UUID BT_MODULE_SERVICE = UUID.fromString("0000bee1-0000-1000-8000-00805f9b34fb");

    private static final UUID BT_MODULE_UUID = UUID.fromString("0000bee2-0000-1000-8000-00805f9b34fb");

    // must be this weird UUID!!!
    public static final UUID BT_CONFIG = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb");

    private static final String TAG = "BT";
    // private VelocityTracker mVelocityTracker = null;

    private BluetoothManager mBluetoothManager;
    private BluetoothGattServer mBluetoothGattServer = null;
    private BluetoothLeAdvertiser mBluetoothLeAdvertiser;

    /* Collection of notification subscribers */
    private Set<BluetoothDevice> mRegisteredDevices = new HashSet<>();

    private TextView tv = null;

    private String _BLEMessage = "";
    private int[] buttonState = {0, 0};
    private float[] orientationQuaternion = {0,0,0,0};
    private SensorManager mSensorManager;

    private ByteBuffer _buffer = ByteBuffer.allocate(32).order(ByteOrder.LITTLE_ENDIAN);

    private final int offset_buttonState1 = 0;
    private final int offset_buttonState2 = 4;
    private final int offset_motionX = 8;
    private final int offset_motionY = 12;
    private final int offset_quatX = 16;
    private final int offset_quatY = 20;
    private final int offset_quatZ = 24;
    private final int offset_quatW = 28;
    //struct IPCMotionState
    //{
    //  int32_t buttonState[2];
    //  float motionXY[2];
    //  float orientationQuaternion[4];
    //};


    public void checkPermission(String permission, int requestCode)
    {
        // Checking if permission is not granted
        if (ContextCompat.checkSelfPermission(this, permission) == PackageManager.PERMISSION_DENIED) {
            ActivityCompat.requestPermissions(this, new String[] { permission }, requestCode);
        }
        else
        {
            createScene();
        //    Toast.makeText(this, "Permission already granted", Toast.LENGTH_SHORT).show();
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions,
                                           int[] grantResults) {
        super.onRequestPermissionsResult(requestCode,permissions,grantResults);

        switch (requestCode) {
            case 1:
                // If request is cancelled, the result arrays are empty.
                if (grantResults.length > 0 &&
                        grantResults[0] == PackageManager.PERMISSION_GRANTED) {

                    createScene();

                }  else {
                    // Explain to the user that the feature is unavailable because
                    // the feature requires a permission that the user has denied.
                    // At the same time, respect the user's decision. Don't link to
                    // system settings in an effort to convince the user to change
                    // their decision.
                }
                return;
        }
        // Other 'case' lines to check for other
        // permissions this app might request.
    }


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Log.d(TAG, "AndroidBTTest");

        // initialize your android device sensor capabilities
        mSensorManager = (SensorManager) getSystemService(SENSOR_SERVICE);

        //setContentView(R.layout.activity_main);
        checkPermission("android.permission.BLUETOOTH_ADVERTISE", 1);
    }

    private BluetoothGattService CreateGattService()
    {
        BluetoothGattService service = new BluetoothGattService(BT_MODULE_SERVICE,
                BluetoothGattService.SERVICE_TYPE_PRIMARY);

        // Current Time characteristic
        BluetoothGattCharacteristic currentTime = new BluetoothGattCharacteristic(BT_MODULE_UUID,
                //Read-only characteristic, supports notifications
                BluetoothGattCharacteristic.PROPERTY_READ | BluetoothGattCharacteristic.PROPERTY_NOTIFY,
                BluetoothGattCharacteristic.PERMISSION_READ);

        BluetoothGattDescriptor configDescriptor = new BluetoothGattDescriptor(BT_CONFIG,
                //Read/write descriptor
                BluetoothGattDescriptor.PERMISSION_READ | BluetoothGattDescriptor.PERMISSION_WRITE);
        currentTime.addDescriptor(configDescriptor);

        service.addCharacteristic(currentTime);
        return service;
    }

    private BluetoothGattServerCallback mGattServerCallback = new BluetoothGattServerCallback() {

        @Override
        public void onConnectionStateChange(BluetoothDevice device, int status, int newState) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                Log.i(TAG, "BluetoothDevice CONNECTED: " + device);
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                Log.i(TAG, "BluetoothDevice DISCONNECTED: " + device);
                //Remove device from any active subscriptions
                mRegisteredDevices.remove(device);
            }
        }

        @Override
        public void onCharacteristicWriteRequest(BluetoothDevice device, int requestId,
                                                 BluetoothGattCharacteristic characteristic,
                                                 boolean preparedWrite, boolean responseNeeded,
                                                 int offset, byte[] value) {

            Log.i(TAG, "onCharacteristicWriteRequest");
        }

        @Override
        public void onCharacteristicReadRequest(BluetoothDevice device, int requestId, int offset,
                                                BluetoothGattCharacteristic characteristic) {
            if (BT_MODULE_UUID.equals(characteristic.getUuid())) {
                Log.i(TAG, "Read CurrentTime");
                mBluetoothGattServer.sendResponse(device,
                        requestId,
                        BluetoothGatt.GATT_SUCCESS,
                        0,
                        _buffer.array());
            } else {
                // Invalid characteristic
                Log.w(TAG, "Invalid Characteristic Read: " + characteristic.getUuid());
                mBluetoothGattServer.sendResponse(device,
                        requestId,
                        BluetoothGatt.GATT_FAILURE,
                        0,
                        null);
            }
        }

        @Override
        public void onDescriptorReadRequest(BluetoothDevice device, int requestId, int offset,
                                            BluetoothGattDescriptor descriptor) {
            if (BT_CONFIG.equals(descriptor.getUuid())) {
                Log.d(TAG, "Config descriptor read");
                byte[] returnValue;
                if (mRegisteredDevices.contains(device)) {
                    returnValue = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE;
                } else {
                    returnValue = BluetoothGattDescriptor.DISABLE_NOTIFICATION_VALUE;
                }
                mBluetoothGattServer.sendResponse(device,
                        requestId,
                        BluetoothGatt.GATT_FAILURE,
                        0,
                        returnValue);
            } else {
                Log.w(TAG, "Unknown descriptor read request");
                mBluetoothGattServer.sendResponse(device,
                        requestId,
                        BluetoothGatt.GATT_FAILURE,
                        0,
                        null);
            }
        }

        @Override
        public void onDescriptorWriteRequest(BluetoothDevice device, int requestId,
                                             BluetoothGattDescriptor descriptor,
                                             boolean preparedWrite, boolean responseNeeded,
                                             int offset, byte[] value) {

            Log.w(TAG, "onDescriptorWriteRequest: " + value.length + value[0] + value[1]);

            if (BT_CONFIG.equals(descriptor.getUuid())) {
                Log.w(TAG, " - our descriptor hit");

                if (Arrays.equals(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE, value)) {
                    Log.w(TAG, "Subscribe device to notifications: " + device);
                    descriptor.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);
                    mRegisteredDevices.add(device);
                } else if (Arrays.equals(BluetoothGattDescriptor.DISABLE_NOTIFICATION_VALUE, value)) {
                    Log.w(TAG, "Unsubscribe device from notifications: " + device);
                    descriptor.setValue(BluetoothGattDescriptor.DISABLE_NOTIFICATION_VALUE);
                    mRegisteredDevices.remove(device);
                }

                if(mRegisteredDevices.isEmpty()) tv.setText("No Connection");
                else tv.setText("Connected");

                if (responseNeeded) {
                    mBluetoothGattServer.sendResponse(device,
                            requestId,
                            BluetoothGatt.GATT_SUCCESS,
                            0,
                            null);
                }
            } else {
                Log.w(TAG, "Unknown descriptor write request");
                if (responseNeeded) {
                    mBluetoothGattServer.sendResponse(device,
                            requestId,
                            BluetoothGatt.GATT_FAILURE,
                            0,
                            null);
                }
            }
        }
    };

    private void startServer() {
        mBluetoothGattServer = mBluetoothManager.openGattServer(this, mGattServerCallback);
        if (mBluetoothGattServer == null) {
            Log.w(TAG, "Unable to create GATT server");
            return;
        }

        mBluetoothGattServer.addService(CreateGattService());
        Log.w(TAG, "Started GATT servivce");
    }

    /**
     * Shut down the GATT server.
     */
    private void stopServer() {
        if (mBluetoothGattServer == null) return;

        mBluetoothGattServer.close();
        mBluetoothGattServer = null;

        Log.w(TAG, "Stopped GATT servivce");
    }

    private BroadcastReceiver mBluetoothReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            int state = intent.getIntExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.STATE_OFF);

            switch (state) {
                case BluetoothAdapter.STATE_ON:
                    ActivateBlueToothAndAdvertise();
                    break;
                case BluetoothAdapter.STATE_OFF:
                    DeactivateBlueToothAndAdvertise();
                    break;
                default:
                    // Do nothing
            }

        }
    };


    private AdvertiseCallback mAdvertiseCallback = new AdvertiseCallback() {
        @Override
        public void onStartSuccess(AdvertiseSettings settingsInEffect) {
            Log.i(TAG, "LE Advertise Started.");
        }

        @Override
        public void onStartFailure(int errorCode) {
            Log.w(TAG, "LE Advertise Failed: "+errorCode);
        }
    };

    private void startAdvertising()
    {
        BluetoothAdapter bluetoothAdapter = mBluetoothManager.getAdapter();
        mBluetoothLeAdvertiser = bluetoothAdapter.getBluetoothLeAdvertiser();
        if (mBluetoothLeAdvertiser == null) {
            Log.w(TAG, "Failed to create advertiser");
            return;
        }

        AdvertiseSettings settings = new AdvertiseSettings.Builder()
                .setAdvertiseMode( AdvertiseSettings.ADVERTISE_MODE_LOW_LATENCY )
                .setTxPowerLevel( AdvertiseSettings.ADVERTISE_TX_POWER_HIGH )
                .setConnectable( true )
                .build();

        ParcelUuid pUuid = new ParcelUuid( BT_MODULE_SERVICE );
        AdvertiseData data = new AdvertiseData.Builder()
                .setIncludeDeviceName( false )
                .addServiceUuid( pUuid )
                .addServiceData( pUuid, "Data".getBytes() )
                .build();

        mBluetoothLeAdvertiser
                .startAdvertising(settings, data, mAdvertiseCallback);
        Log.w(TAG, "Is Advertising services");
    }

    private void stopAdvertising() {
        if (mBluetoothLeAdvertiser == null) return;

        mBluetoothLeAdvertiser.stopAdvertising(mAdvertiseCallback);

        Log.w(TAG, "Stopped Advertising services");
    }

    private void notifyRegisteredDevices() {
        if (mRegisteredDevices.isEmpty()) {
            //Log.i(TAG, "No subscribers registered");
            return;
        }

        BluetoothGattCharacteristic positionValue = mBluetoothGattServer
                .getService(BT_MODULE_SERVICE)
                .getCharacteristic(BT_MODULE_UUID);
        positionValue.setValue(_buffer.array());

        Log.i(TAG, "Sending update to " + mRegisteredDevices.size() + " subscribers");
        for (BluetoothDevice device : mRegisteredDevices) {
            mBluetoothGattServer.notifyCharacteristicChanged(device, positionValue, false);
        }
    }

    @Override
    protected void onStart() {
        super.onStart();

        Log.w(TAG, "onStart");

        // Register for system Bluetooth events
        IntentFilter filter = new IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED);
        registerReceiver(mBluetoothReceiver, filter);

        ActivateBlueToothAndAdvertise();

        mSensorManager.registerListener(this,
                mSensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER),
                SensorManager.SENSOR_DELAY_GAME);
        mSensorManager.registerListener(this,
                mSensorManager.getDefaultSensor(Sensor.TYPE_MAGNETIC_FIELD),
                SensorManager.SENSOR_DELAY_GAME);
    }

    @Override
    protected void onStop() {
        super.onStop();

        Log.w(TAG, "onStop");

        unregisterReceiver(mBluetoothReceiver);

        DeactivateBlueToothAndAdvertise();

        // to stop the listener and save battery
        mSensorManager.unregisterListener(this,mSensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER));
        mSensorManager.unregisterListener(this,mSensorManager.getDefaultSensor(Sensor.TYPE_MAGNETIC_FIELD));
    }


    @Override
    protected void onDestroy() {
        super.onDestroy();

        BluetoothAdapter bluetoothAdapter = mBluetoothManager.getAdapter();
        if (bluetoothAdapter.isEnabled()) {
            stopServer();
            stopAdvertising();
        }

    }

    private boolean checkBluetoothSupport(BluetoothAdapter bluetoothAdapter) {

        if (bluetoothAdapter == null) {
            Log.w(TAG, "Bluetooth is not supported");
            return false;
        }

        if (!getPackageManager().hasSystemFeature(PackageManager.FEATURE_BLUETOOTH_LE)) {
            Log.w(TAG, "Bluetooth LE is not supported");
            return false;
        }

        return true;
    }

    private void createScene()
    {
        Log.w(TAG, "createScene");

        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        /* Create a TextView and set its text to "Hello world" */
        tv = new TextView(this);
        tv.setText("Startup...");

        layout.addView(tv);

        setContentView(layout);

        // Devices with a display should not go to sleep
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        mBluetoothManager = (BluetoothManager) getSystemService(BLUETOOTH_SERVICE);
        BluetoothAdapter bluetoothAdapter = mBluetoothManager.getAdapter();
        // We can't continue without proper Bluetooth support
        //if (!checkBluetoothSupport(bluetoothAdapter)) {
          //  finish();
        //}

        if (!getPackageManager().hasSystemFeature(PackageManager.FEATURE_BLUETOOTH_LE)) {
            tv.setText("Bluetooth LE is not supported");
            return;
        }


        if (!bluetoothAdapter.isEnabled()) {
            tv.setText("Bluetooth is currently disabled...");
        }
        else {
            tv.setText("Active");
            ActivateBlueToothAndAdvertise();
        }
    }

    private boolean bIsActive = false;

    private void ActivateBlueToothAndAdvertise()
    {
        if(bIsActive || mBluetoothManager == null) return;
        bIsActive = true;
        startAdvertising();
        startServer();
    }

    private void DeactivateBlueToothAndAdvertise()
    {
        if(!bIsActive || mBluetoothManager == null) return;
        bIsActive = false;
        stopServer();
        stopAdvertising();
    }

    float[] mGravity;
    float[] mGeomagnetic;
    public void onSensorChanged(SensorEvent event) {
        if (event.sensor.getType() == Sensor.TYPE_ACCELEROMETER)
            mGravity = event.values;
        if (event.sensor.getType() == Sensor.TYPE_MAGNETIC_FIELD)
            mGeomagnetic = event.values;
        if (mGravity != null && mGeomagnetic != null) {
            float R[] = new float[9];
            float I[] = new float[9];
            /*Computes the inclination matrix I as well as the rotation matrix R transforming a vector from the device coordinate
             *  system to the world's coordinate system which is defined as a direct orthonormal basis*/
            boolean success = SensorManager.getRotationMatrix(R, I, mGravity, mGeomagnetic);
            if (success) {
                float mOrientation[] = new float[3];
                SensorManager.getOrientation(R, mOrientation);
                SensorManager.getQuaternionFromVector(orientationQuaternion, mOrientation);

                _buffer.putFloat(offset_quatX, orientationQuaternion[0]);
                _buffer.putFloat(offset_quatY, orientationQuaternion[1]);
                _buffer.putFloat(offset_quatZ, orientationQuaternion[2]);
                _buffer.putFloat(offset_quatW, orientationQuaternion[3]);
                notifyRegisteredDevices();
            }
        }
    }

    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) {
        // Do something here if sensor accuracy changes.
        // You must implement this callback in your code.
        //if (sensor == mValuen) {
            switch (accuracy) {
                case 0:
                    System.out.println("Unreliable");
                    break;
                case 1:
                    System.out.println("Low Accuracy");
                    break;
                case 2:
                    System.out.println("Medium Accuracy");
                    break;
                case 3:
                    System.out.println("High Accuracy");
                    break;
            }
        //}
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        int action = event.getAction() & MotionEvent.ACTION_MASK;
        int pointerIndex = (event.getAction() & MotionEvent.ACTION_POINTER_INDEX_MASK) >> MotionEvent.ACTION_POINTER_INDEX_SHIFT;
        int pointerId = event.getPointerId(pointerIndex);

        if (pointerId > 1 || pointerId < 0)
        {
            return false;
        }

        switch(action) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_POINTER_DOWN:
            case MotionEvent.ACTION_MOVE:
                _buffer.putInt(offset_buttonState1 + pointerId*4, 1);
                _buffer.putFloat(offset_motionX, event.getX());
                _buffer.putFloat(offset_motionY, event.getY());
                notifyRegisteredDevices();
                break;

            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP:
            case MotionEvent.ACTION_CANCEL:
                _buffer.putInt(offset_buttonState1 + pointerId*4, 0);
                notifyRegisteredDevices();
                break;
        }

        return true;
    }
}