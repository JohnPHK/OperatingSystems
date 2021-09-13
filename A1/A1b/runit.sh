make
MOUNT_DIR="mnt"
mkdir $MOUNT_DIR/

echo "Create an image, format the image and mount the image"
truncate -s 20M img
./mkfs.a1fs -i 200 img
./a1fs img $MOUNT_DIR/

echo "The overall a1fs file system:"
stat -f $MOUNT_DIR/
echo

echo "Make three directory and check it is created:"
mkdir ./$MOUNT_DIR/dir_1
mkdir ./$MOUNT_DIR/dir_2
mkdir ./$MOUNT_DIR/dir_3
ls -al ./$MOUNT_DIR/
echo

echo "Remove directory 1 and check that it is deleted:"
rmdir ./$MOUNT_DIR/dir_1
ls -al ./$MOUNT_DIR
echo

echo "Create file one, two, three in directory 2 and check the content of the directory2 containing it:"
touch ./$MOUNT_DIR/dir_2/one
touch ./$MOUNT_DIR/dir_2/two
touch ./$MOUNT_DIR/dir_2/three
ls -al ./$MOUNT_DIR/dir_2
echo

echo "Unlink file one and check directory 2"
rm ./$MOUNT_DIR/dir_2/one
ls -al ./$MOUNT_DIR/dir_2
echo

echo "Move file two to directory 3 and check directory 2"
mv ./$MOUNT_DIR/dir_2/two ./$MOUNT_DIR/dir_3/two
ls -al ./$MOUNT_DIR/dir_2
echo

echo "check directory 3"
ls -al ./$MOUNT_DIR/dir_3
echo

echo "Test truncate, creating a file four in directory2 with size 100:"
truncate -s 100 ./$MOUNT_DIR/dir_2/four
ls -al ./$MOUNT_DIR/dir_2
echo

echo "Write data to the file three"
echo "Some data written" >> ./$MOUNT_DIR/dir_2/three
ls -al ./$MOUNT_DIR/dir_2
echo


echo "Read the file we wrote on:"
cat ./$MOUNT_DIR/dir_2/three
echo


echo "Overview of the current file system"
stat -f $MOUNT_DIR/
echo


echo "unmount"
fusermount -u $MOUNT_DIR/
echo


echo "mount again"
./a1fs img $MOUNT_DIR/
echo

echo "display the root directory"
ls -al $MOUNT_DIR/
echo

echo "display the directory 2"
ls -al $MOUNT_DIR/dir_2
echo

echo "Overview of the current file system"
stat -f $MOUNT_DIR/
echo

sleep 0.1 
echo "unmount"
fusermount -u $MOUNT_DIR/
