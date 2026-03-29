namespace bin
{
	/** IReader over a complete byte buffer (no Unity dependency). */
	public sealed class TestMemReader : IReader
	{
		private readonly byte[] buffer_;
		private uint rdptr_;

		public TestMemReader(byte[] buffer)
		{
			buffer_ = buffer;
			rdptr_ = 0;
		}

		public bool read(uint len, out byte[] data, out int startId)
		{
			data = buffer_;
			startId = (int)rdptr_;
			if (buffer_ == null || rdptr_ + len > (uint)buffer_.Length)
				return false;
			rdptr_ += len;
			return true;
		}
	}
}
