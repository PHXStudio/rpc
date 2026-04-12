using System;
using System.Collections.Generic;
using UnityEngine;
namespace rpc
{
    public class MemWriter : rpc.IWriter
    {
        public MemWriter()
        {
            buffer_ = new List<byte>();
        }
        public byte[] buffer { get { return buffer_.ToArray(); } }
    	public void writeBegin()
        {
        }
    	public void write(byte[] data)
        {
            foreach (byte b in data)
                buffer_.Add(b);
        }
        public void writeEnd()
        {
        }
        private List<byte> buffer_;
    }
}