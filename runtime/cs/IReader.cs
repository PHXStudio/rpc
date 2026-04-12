namespace bin 
{
    /** Abstract interface for reading binary data. */
    public interface IReader
    {
        /**
             * @param size data size want to read.
             * @param[out] data buffer.
             * @param[out] data buffer start id.
             */
        bool read(uint size, out byte[] data, out int startId);

        /** Skip specified number of bytes in the stream.
            Used for version compatibility to skip unknown fields.
            @param len Number of bytes to skip.
            @return true if successful, false if not enough data.
         */
        bool Skip(uint len);
    }
}
