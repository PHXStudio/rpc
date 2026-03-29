using System.Collections.Generic;

namespace bin
{
	/** IWriter backed by a growable buffer (no Unity dependency). */
	public sealed class TestMemWriter : IWriter
	{
		private readonly List<byte> buffer_ = new List<byte>();

		public void write(byte[] data)
		{
			if (data == null || data.Length == 0)
				return;
			buffer_.AddRange(data);
		}

		public byte[] ToArray()
		{
			return buffer_.ToArray();
		}
	}
}
